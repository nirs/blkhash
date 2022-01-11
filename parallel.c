// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <pthread.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <assert.h>
#include <sys/queue.h>
#include <unistd.h>

#include "blkhash.h"
#include "blksum.h"
#include "util.h"

/* Maximum number of commands per worker. */
#define MAX_COMMANDS 16

struct job {
    char *uri;
    uint64_t size;
    struct options *opt;
    const EVP_MD *md;
    int md_size;
    size_t segment_count;
    pthread_mutex_t mutex;
    size_t segment;
    unsigned char *digests_buffer;

    /* Set if job started a nbd server to serve filename. */
    struct nbd_server *nbd_server;

    /* Set if progress is enabled. */
    struct progress *progress;
};

struct extent_array {
    struct extent *array;
    size_t count;
    size_t index;
};

struct command {
    STAILQ_ENTRY(command) entry;
    uint64_t seq;
    uint64_t started;
    void *buf;
    uint32_t length;
    int wid;
    bool ready;
    bool zero;
};

struct command_queue {
    STAILQ_HEAD(, command) head;
    int len;    /* Number of items in queue. */
    int bytes;  /* Total length of non-zero commands. */
    int size;   /* Maximum length of non-zero commands. */
};

struct worker {
    pthread_t thread;
    int id;
    struct job *job;
    struct src *s;
    struct blkhash *h;
    struct extent_array extents;
    struct command_queue queue;

    /* Ensure that we process commands in order. */
    uint64_t cmd_queued;
    uint64_t cmd_processed;
};

static inline uint32_t cost(bool zero, uint32_t len)
{
    return zero ? 0 : len;
}

static inline void queue_init(struct command_queue *q, int size)
{
    STAILQ_INIT(&q->head);
    q->len = 0;
    q->bytes = 0;
    q->size = size;
}

static inline void queue_push(struct command_queue *q, struct command *cmd)
{
    assert(q->len < MAX_COMMANDS);
    q->len++;

    q->bytes += cost(cmd->zero, cmd->length);
    assert(q->bytes <= q->size);

    STAILQ_INSERT_TAIL(&q->head, cmd, entry);
}

static inline struct command *queue_pop(struct command_queue *q)
{
    struct command *cmd;

    cmd = STAILQ_FIRST(&q->head);
    STAILQ_REMOVE_HEAD(&q->head, entry);

    assert(q->len > 0);
    q->len--;

    q->bytes -= cost(cmd->zero, cmd->length);
    assert(q->bytes >= 0);

    return cmd;
}

static inline bool queue_ready(struct command_queue *q)
{
    return q->len > 0 && STAILQ_FIRST(&q->head)->ready;
}

static inline int can_push(struct command_queue *q, struct extent *extent)
{
    return q->len < MAX_COMMANDS &&
	   q->size - q->bytes >= cost(extent->zero, extent->length);
}

static void optimize(const char *filename, struct options *opt,
                     struct file_info *fi)
{
    if (fi->fs_name && strcmp(fi->fs_name, "nfs") == 0) {
        /*
         * Direct I/O can be up to 11X times slower on NFS, and using host
         * page cache should always be correct on NFS.
         */
        if (!opt->cache) {
            opt->cache = true;
            DEBUG("Optimize for 'nfs': cache=yes");
        }

        /*
         * For raw format large queue and read sizes can be 2.5x times
         * faster. For qcow2, the default values give best performance.
         */
        if (strcmp(fi->format, "raw") == 0) {
            if ((opt->flags & USER_READ_SIZE) == 0) {
                opt->read_size = 2 * 1024 * 1024;
                DEBUG("Optimize for 'raw' image on 'nfs': read_size=%ld",
                      opt->read_size);
            }

            if ((opt->flags & USER_QUEUE_SIZE) == 0) {
                opt->queue_size = 4 * 1024 * 1024;
                if (opt->queue_size < opt->read_size)
                    opt->queue_size = opt->read_size;
                DEBUG("Optimize for 'raw' image on 'nfs': queue_size=%ld",
                      opt->queue_size);
            }
        }
    } else {
        /*
         * For other storage, direct I/O is required for correctness on
         * some cases (e.g. LUN connected to multiple hosts), and typically
         * faster and more consitent. However it is not supported on all
         * filesystems so we must check if file can be used with direct
         * I/O.
         */
        if (!opt->cache && !supports_direct_io(filename)) {
            opt->cache = true;
            DEBUG("Optimize for '%s' image on '%s': cache=yes",
                  fi->format, fi->fs_name);
        }
    }
}

static void init_job(struct job *job, const char *filename,
                     struct options *opt)
{
    int err;
    struct src *src;

    if (is_nbd_uri(filename)) {
        /* Use user provided nbd server. */
        job->uri = strdup(filename);
        if (job->uri == NULL)
            FAIL_ERRNO("strdup");
    } else {
        struct file_info fi = {0};
        /*
         * If we have NBD, start nbd server and use nbd uri. Otherwise use file
         * directly if it is a raw format.
         */
        probe_file(filename, &fi);

#ifdef HAVE_NBD
        optimize(filename, opt, &fi);

        struct server_options options = {
            .filename=filename,
            .format=fi.format,
            .cache=opt->cache,
            .workers=opt->workers,
        };

        job->nbd_server = start_nbd_server(&options);
        job->uri = nbd_server_uri(job->nbd_server);
#else
        if (strcmp(fi.format, "raw") != 0)
            FAIL("%s format requires NBD", fi.format);

        job->uri = strdup(filename);
        if (job->uri == NULL)
            FAIL_ERRNO("strdup");
#endif
    }

    /* Connect to source for getting size. */
    src = open_src(job->uri);
    job->size = src->size;
    src_close(src);

    /* Initalize job. */
    job->opt = opt;
    job->md = EVP_get_digestbyname(opt->digest_name);
    if (job->md == NULL)
        FAIL_ERRNO("EVP_get_digestbyname");

    job->md_size = EVP_MD_size(job->md);
    job->segment_count = (job->size + opt->segment_size - 1)
        / opt->segment_size;
    err = pthread_mutex_init(&job->mutex, NULL);
    if (err)
        FAIL("pthread_mutex_init: %s", strerror(err));

    job->segment = 0;
    job->digests_buffer = malloc(job->segment_count * job->md_size);
    if (job->digests_buffer == NULL)
        FAIL_ERRNO("malloc");

    if (opt->progress)
        job->progress = progress_open(job->segment_count);
}

static void destroy_job(struct job *job)
{
    int err;

    err = pthread_mutex_destroy(&job->mutex);
    if (err)
        FAIL("pthread_mutex_destroy: %s", strerror(err));

    free(job->digests_buffer);
    job->digests_buffer = NULL;

    free(job->uri);
    job->uri = NULL;

#ifdef HAVE_NBD
    if (job->nbd_server) {
        stop_nbd_server(job->nbd_server);
        free(job->nbd_server);
        job->nbd_server = NULL;
    }
#endif

    if (job->progress) {
        progress_close(job->progress);
        job->progress = NULL;
    }
}

static inline unsigned char *segment_md(struct job *job, size_t n)
{
    return &job->digests_buffer[n * job->md_size];
}

static ssize_t next_segment(struct job *job)
{
    int err;
    ssize_t segment = -1;

    err = pthread_mutex_lock(&job->mutex);
    if (err)
        FAIL("pthread_mutex_lock: %s", strerror(err));

    if (job->segment < job->segment_count) {
        segment = job->segment++;
    }

    err = pthread_mutex_unlock(&job->mutex);
    if (err)
        FAIL("pthread_mutex_unlock: %s", strerror(err));

    return segment;
}

static void compute_root_hash(struct job *job, unsigned char *out)
{
    struct options *opt = job->opt;
    EVP_MD_CTX *md_ctx;

    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL)
        FAIL_ERRNO("EVP_MD_CTX_new");

    EVP_DigestInit_ex(md_ctx, job->md, NULL);

    for (size_t i = 0; i < job->segment_count; i++) {
        unsigned char *seg_md = segment_md(job, i);

        if (debug) {
            char hex[EVP_MAX_MD_SIZE * 2 + 1];

            format_hex(seg_md, job->md_size, hex);
            DEBUG("segment %ld offset %ld checksum %s",
                  i, i * opt->segment_size, hex);
        }

        EVP_DigestUpdate(md_ctx, seg_md, job->md_size);
    }

    EVP_DigestFinal_ex(md_ctx, out, NULL);

    EVP_MD_CTX_free(md_ctx);
}

static struct command *create_command(int64_t offset, struct extent *extent,
                                      int wid, uint64_t seq)
{
    struct command *c;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        FAIL_ERRNO("calloc");

    if (!extent->zero) {
        c->buf = malloc(extent->length);
        if (c->buf == NULL)
            FAIL_ERRNO("malloc");
    }

    c->seq = seq;

    if (debug)
        c->started = gettime();

    c->length = extent->length;
    c->wid = wid;
    c->ready = extent->zero;
    c->zero = extent->zero;

    return c;
}

static int read_completed(void *user_data, int *error)
{
    struct command *cmd = user_data;

    DEBUG("worker %d command %" PRIu64 " ready in %" PRIu64 " usec",
          cmd->wid, cmd->seq, gettime() - cmd->started);

    cmd->ready = true;
    return 1;
}

static void free_command(struct command *c)
{
    if (c) {
        free(c->buf);
        free(c);
    }
}

static void start_command(struct worker *w, int64_t offset, struct extent *extent)
{
    struct command *cmd;

    DEBUG("worker %d command %" PRIu64 " started offset=%" PRIi64
          " length=%" PRIu32 " zero=%d",
          w->id, w->cmd_queued, offset, extent->length, extent->zero);

    cmd = create_command(offset, extent, w->id, w->cmd_queued);
    queue_push(&w->queue, cmd);
    w->cmd_queued++;

    if (!cmd->zero)
        src_aio_pread(w->s, cmd->buf, extent->length, offset, read_completed, cmd);
}

static void finish_command(struct worker *w)
{
    struct command *cmd = queue_pop(&w->queue);

    assert(cmd->ready);

    /* Esnure we process commands in order. */
    assert(cmd->seq == w->cmd_processed);
    w->cmd_processed++;

    if (!io_only) {
        if (cmd->zero)
            blkhash_zero(w->h, cmd->length);
        else
            blkhash_update(w->h, cmd->buf, cmd->length);
    }

    DEBUG("worker %d command %" PRIu64 " finished in %" PRIu64 " usec "
          "length=%" PRIu32 " zero=%d",
          w->id, cmd->seq, gettime() - cmd->started, cmd->length, cmd->zero);

    free_command(cmd);
}

static void clear_extents(struct worker *w)
{
    if (w->extents.array) {
        free(w->extents.array);
        w->extents.array = NULL;
        w->extents.count = 0;
        w->extents.index = 0;
    }
}

static void fetch_extents(struct worker *w, int64_t offset, uint32_t length)
{
    clear_extents(w);

    DEBUG("worker %d get extents offset=%" PRIi64 " length=%" PRIu32,
          w->id, offset, length);

    src_extents(w->s, offset, length, &w->extents.array, &w->extents.count);

    DEBUG("worker %d got %lu extents", w->id, w->extents.count);
}

static inline bool need_extents(struct worker *w)
{
    return w->extents.index == w->extents.count;
}

static void next_extent(struct worker *w, struct extent *extent)
{
    struct options *opt = w->job->opt;
    struct extent *current;

    assert(w->extents.index < w->extents.count);
    current = &w->extents.array[w->extents.index];

    /* Consume entire zero extent, or up to read size from data extent. */

    extent->zero = current->zero;
    if (extent->zero)
        extent->length = current->length;
    else
        extent->length = current->length < opt->read_size ?
            current->length : opt->read_size;

    current->length -= extent->length;

    DEBUG("worker %d extent %ld zero=%d take=%u left=%u",
          w->id, w->extents.index, extent->zero, extent->length,
          current->length);

    /* Advance to next extent if current is consumed. */

    if (current->length == 0)
        w->extents.index++;
}

static void process_segment(struct worker *w, int64_t offset)
{
    struct job *job = w->job;
    struct options *opt = job->opt;
    int64_t end;
    struct extent extent = {0};

    end = offset + opt->segment_size;
    if (end > job->size)
        end = job->size;

    while (w->queue.len || offset < end) {

        while (offset < end) {

            if (extent.length == 0) {
                if (need_extents(w))
                    fetch_extents(w, offset, end - offset);
                next_extent(w, &extent);
            }

            if (!can_push(&w->queue, &extent))
                break;

            start_command(w, offset, &extent);
            offset += extent.length;
            extent.length = 0;
        }

        src_aio_run(w->s, 1000);

        while (queue_ready(&w->queue))
            finish_command(w);
    }

    clear_extents(w);
}

static void *worker_thread(void *arg)
{
    struct worker *w = (struct worker *)arg;
    struct job *job = w->job;
    struct options *opt = job->opt;
    ssize_t segment;

    DEBUG("worker %d started", w->id);

    w->s = open_src(job->uri);

    w->h = blkhash_new(opt->block_size, opt->digest_name);
    if (w->h == NULL)
        FAIL_ERRNO("blkhash_new");

    while ((segment = next_segment(job)) != -1) {
        int64_t offset = segment * opt->segment_size;
        unsigned char *seg_md = segment_md(job, segment);

        DEBUG("worker %d processing segment %zd offset %" PRIi64,
              w->id, segment, offset);

        process_segment(w, offset);
        blkhash_final(w->h, seg_md, NULL);
        blkhash_reset(w->h);

        if (job->progress)
            progress_update(job->progress, 1);
    }

    blkhash_free(w->h);
    src_close(w->s);

    DEBUG("worker %d finished", w->id);
    return NULL;
}

void parallel_checksum(const char *filename, struct options *opt,
                       unsigned char *out)
{
    struct job job = {0};
    struct worker *workers;
    size_t worker_count;
    int err;

    init_job(&job, filename, opt);

    worker_count = (job.segment_count < opt->workers)
        ? job.segment_count : opt->workers;

    workers = calloc(worker_count, sizeof(*workers));
    if (workers == NULL)
        FAIL_ERRNO("calloc");

    for (int i = 0; i < worker_count; i++) {
        struct worker *w = &workers[i];
        w->id = i;
        w->job = &job;
        queue_init(&w->queue, opt->queue_size);

        DEBUG("starting worker %d", i);
        err = pthread_create(&w->thread, NULL, worker_thread, w);
        if (err)
            FAIL("pthread_create: %s", strerror(err));
    }

    if (job.progress) {
        while (progress_draw(job.progress))
            usleep(100000);
    }

    for (int i = 0; i < worker_count; i++) {
        DEBUG("joining worker %d", i);
        err = pthread_join(workers[i].thread, NULL);
        if (err)
            FAIL("pthread_join: %s", strerror(err));
    }

    compute_root_hash(&job, out);

    free(workers);
    destroy_job(&job);
}
