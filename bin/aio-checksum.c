// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <pthread.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <assert.h>
#include <sys/queue.h>
#include <unistd.h>

#include "blkhash.h"
#include "blkhash-config.h"
#include "blksum.h"
#include "util.h"

/* qemu-nbd process up to 16 in-flight commands per connection. */
#define MAX_COMMANDS 16

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
    bool ready;
    bool zero;
};

struct command_queue {
    STAILQ_HEAD(, command) head;
    int len;       /* Number of items in queue. */
    size_t bytes;  /* Total length of non-zero commands. */
    size_t size;   /* Maximum length of non-zero commands. */
};

struct worker {
    pthread_t thread;

    /* Watched fd for detecting source I/O events. */
    struct pollfd poll_fds[1];

    char *uri;
    struct nbd_server *nbd_server;
    struct options *opt;
    struct src *s;
    int64_t image_size;
    struct blkhash *h;
    struct extent_array extents;
    struct command_queue queue;

    /* Ensure that we process commands in order. */
    uint64_t commands_started;

    /* The computed checksum. */
    unsigned char *out;
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
    uint32_t cmd_cost;

    cmd = STAILQ_FIRST(&q->head);
    STAILQ_REMOVE_HEAD(&q->head, entry);

    assert(q->len > 0);
    q->len--;

    cmd_cost = cost(cmd->zero, cmd->length);
    assert(q->bytes >= cmd_cost);
    q->bytes -= cmd_cost;

    return cmd;
}

static inline bool has_commands(struct worker *w)
{
    return w->queue.len > 0;
}

static inline bool has_ready_command(struct worker *w)
{
    struct command_queue *q = &w->queue;

    return q->len > 0 && STAILQ_FIRST(&q->head)->ready;
}

static inline int can_process(struct worker *w, struct extent *extent)
{
    struct command_queue *q = &w->queue;

    return q->len < MAX_COMMANDS &&
	   q->size - q->bytes >= cost(extent->zero, extent->length);
}

static void optimize(const char *filename, struct options *opt,
                     struct file_info *fi)
{
    if (fi->fs_name && strcmp(fi->fs_name, "nfs") == 0) {
        /*
         * cache=false aio=native can be up to 1180% slower. cache=false
         * aio=threads can be 36% slower, but may be wanted to avoid
         * polluting the page cache with data that is not going to be
         * used.
         */
        if (strcmp(opt->aio, "native") == 0) {
            opt->aio = "threads";
            DEBUG("Optimize for 'nfs': aio=threads");
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
         * faster and more consistent. However it is not supported on all
         * file systems so we must check if file can be used with direct
         * I/O.
         */
        if (!opt->cache && !supports_direct_io(filename)) {
            opt->cache = true;
            DEBUG("Optimize for '%s' image on '%s': cache=yes",
                  fi->format, fi->fs_name);
        }

        /* Cache is not compatible with aio=native. */
        if (opt->cache && strcmp(opt->aio, "native") == 0) {
            opt->aio = "threads";
            DEBUG("Optimize for '%s' image on '%s': aio=threads",
                  fi->format, fi->fs_name);
        }
    }
}

static struct command *create_command(struct extent *extent, uint64_t seq)
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
    c->ready = extent->zero;
    c->zero = extent->zero;

    return c;
}

static int read_completed(void *user_data, int *error)
{
    struct command *cmd = user_data;

    /*
     * This is not documented, but *error is errno value translated from
     * the NBD server error.
     */
    if (*error)
        FAIL("Read failed: %s", strerror(*error));

    DEBUG("Command %" PRIu64 " ready in %" PRIu64 " usec",
          cmd->seq, gettime() - cmd->started);

    cmd->ready = true;

    /* Required for linbd to "retire" the command. */
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

    DEBUG("Command %" PRIu64 " started offset=%" PRIi64
          " length=%" PRIu32 " zero=%d",
          w->commands_started, offset, extent->length, extent->zero);

    cmd = create_command(extent, w->commands_started);
    queue_push(&w->queue, cmd);
    w->commands_started++;

    if (!cmd->zero)
        src_aio_pread(w->s, cmd->buf, extent->length, offset, read_completed, cmd);
}

static void finish_command(struct worker *w)
{
    struct command *cmd = queue_pop(&w->queue);

    assert(cmd->ready);

    if (!io_only) {
        int err;
        if (cmd->zero) {
            err = blkhash_zero(w->h, cmd->length);
            if (err)
                FAIL("blkhash_zero: %s", strerror(err));
        } else {
            err = blkhash_update(w->h, cmd->buf, cmd->length);
            if (err)
                FAIL("blkhash_update: %s", strerror(err));
        }
    }

    if (w->opt->progress)
        progress_update(cmd->length);

    DEBUG("Command %" PRIu64 " finished in %" PRIu64 " usec "
          "length=%" PRIu32 " zero=%d",
          cmd->seq, gettime() - cmd->started, cmd->length, cmd->zero);

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
    uint64_t start = 0;
    clear_extents(w);

    DEBUG("Get extents offset=%" PRIi64 " length=%" PRIu32,
          offset, length);

    if (debug)
        start = gettime();

    src_extents(w->s, offset, length, &w->extents.array, &w->extents.count);

    DEBUG("Got %lu extents in %" PRIu64 " usec",
          w->extents.count, gettime() - start);
}

static inline bool need_extents(struct worker *w)
{
    return w->extents.index == w->extents.count;
}

static void next_extent(struct worker *w, int64_t offset,
                        struct extent *extent)
{
    struct extent *current;

    if (need_extents(w)) {
        uint32_t length = MIN(w->opt->extents_size, w->image_size - offset);
        fetch_extents(w, offset, length);
    }

    assert(w->extents.index < w->extents.count);
    current = &w->extents.array[w->extents.index];

    /* Consume entire zero extent, or up to read size from data extent. */

    extent->zero = current->zero;
    if (extent->zero)
        extent->length = current->length;
    else
        extent->length = current->length < w->opt->read_size ?
            current->length : w->opt->read_size;

    current->length -= extent->length;

    DEBUG("Extent %ld zero=%d take=%u left=%u",
          w->extents.index, extent->zero, extent->length,
          current->length);

    /* Advance to next extent if current is consumed. */

    if (current->length == 0)
        w->extents.index++;
}

static int wait_for_events(struct worker *w)
{
    int n;

    if (src_aio_prepare(w->s, &w->poll_fds[0]))
        return -1;

    /* If the fd is -1, the source does not need polling in this iteration.
     * Polling on this fd blocks until the timeout expires. */
    if (w->poll_fds[0].fd == -1)
        return 0;

    do {
        n = poll(w->poll_fds, 1, -1);
    } while (n == -1 && errno == EINTR);

    if (n == -1) {
        ERROR("Polling failed: %s", strerror(errno));
        return -1;
    }

    if (w->poll_fds[0].revents)
        return src_aio_notify(w->s, &w->poll_fds[0]);

    return 0;
}

static void process_image(struct worker *w)
{
    int64_t offset = 0;
    struct extent extent = {0};

    while (has_commands(w) || offset < w->image_size) {

        while (offset < w->image_size) {

            if (extent.length == 0)
                next_extent(w, offset, &extent);

            if (!can_process(w, &extent))
                break;

            start_command(w, offset, &extent);
            offset += extent.length;
            assert(offset <= w->image_size);
            extent.length = 0;
        }

        if (wait_for_events(w))
            FAIL("Worker failed");

        while (has_ready_command(w))
            finish_command(w);

        if (!running()) {
            DEBUG("Worker aborting");
            break;
        }
    }

    clear_extents(w);
}

static void *worker_thread(void *arg)
{
    struct worker *w = (struct worker *)arg;
    int err = 0;

    DEBUG("Worker started");

    w->s = open_src(w->uri);
    w->image_size = w->s->size;

    if (w->opt->progress)
        progress_init(w->image_size);

    process_image(w);

    if (running())
        err = blkhash_final(w->h, w->out, NULL);

    src_close(w->s);

    if (w->opt->progress)
        progress_clear();

    if (err)
        FAIL("blkhash_final: %s", strerror(err));

    DEBUG("Worker finished");
    return NULL;
}

static void create_hash(struct worker *w)
{
    struct blkhash_opts *ho;

    ho = blkhash_opts_new(w->opt->digest_name);
    if (ho == NULL)
        FAIL_ERRNO("blkhash_opts_new");

    if (blkhash_opts_set_block_size(ho, w->opt->block_size))
        FAIL("Invalid block size value: %zu", w->opt->block_size);

    if (blkhash_opts_set_streams(ho, w->opt->streams))
        FAIL("Invalid streams value: %zu", w->opt->streams);

    if (blkhash_opts_set_threads(ho, w->opt->threads))
        FAIL("Invalid threads value: %zu", w->opt->threads);

    w->h = blkhash_new_opts(ho);
    blkhash_opts_free(ho);
    if (w->h == NULL)
        FAIL_ERRNO("blkhash_new");
}

static void init_worker(struct worker *w, const char *filename,
                        struct options *opt, unsigned char *out)
{
    w->opt = opt;
    w->out = out;

    if (is_nbd_uri(filename)) {
        /* Use user provided nbd server. */
        w->uri = strdup(filename);
        if (w->uri == NULL)
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
            .aio=opt->aio,
            .cache=opt->cache,
        };

        w->nbd_server = start_nbd_server(&options);
        w->uri = nbd_server_uri(w->nbd_server);
#else
        if (strcmp(fi.format, "raw") != 0)
            FAIL("%s format requires NBD", fi.format);

        w->uri = strdup(filename);
        if (w->uri == NULL)
            FAIL_ERRNO("strdup");
#endif
    }

    queue_init(&w->queue, w->opt->queue_size);
    create_hash(w);
}

static void start_worker(struct worker *w)
{
    int err;

    DEBUG("starting worker");
    err = pthread_create(&w->thread, NULL, worker_thread, w);
    if (err)
        FAIL("pthread_create: %s", strerror(err));
}

static void join_worker(struct worker *w)
{
    int err;

    DEBUG("joining worker");
    err = pthread_join(w->thread, NULL);
    if (err)
        FAIL("pthread_join: %s", strerror(err));
}

static void destroy_worker(struct worker *w)
{
    blkhash_free(w->h);
    free(w->uri);

#ifdef HAVE_NBD
    if (w->nbd_server) {
        stop_nbd_server(w->nbd_server);
        free(w->nbd_server);
    }
#endif
}

void aio_checksum(const char *filename, struct options *opt,
                  unsigned char *out)
{
    struct worker w = {0};

    init_worker(&w, filename, opt, out);
    start_worker(&w);
    join_worker(&w);
    destroy_worker(&w);
}
