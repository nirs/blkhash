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

struct extent_array {
    struct extent *array;
    size_t count;
    size_t index;
};

struct command {
    STAILQ_ENTRY(command) entry;
    void *buf;
    int64_t offset;
    uint64_t started;
    uint32_t length;
    bool completed;
    bool zero;
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
    int64_t read_offset;
    int64_t bytes_hashed;
    struct blkhash *h;
    struct extent_array extents;
    STAILQ_HEAD(, command) read_queue;
    unsigned commands_in_flight;

    /* The computed checksum. */
    unsigned char *out;
};

static inline void push_command(struct worker *w, struct command *cmd)
{
    assert(w->commands_in_flight < w->opt->queue_depth);
    STAILQ_INSERT_TAIL(&w->read_queue, cmd, entry);
    w->commands_in_flight++;
}

static inline struct command *pop_command(struct worker *w)
{
    struct command *cmd;

    assert(w->commands_in_flight > 0);
    cmd = STAILQ_FIRST(&w->read_queue);
    STAILQ_REMOVE_HEAD(&w->read_queue, entry);
    w->commands_in_flight--;

    return cmd;
}

static inline bool has_data(struct worker *w)
{
    struct command *next = STAILQ_FIRST(&w->read_queue);
    return next && next->completed;
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
            /* If user did not specify read size, use larger value. */
            if ((opt->flags & USER_READ_SIZE) == 0) {
                opt->read_size = 2 * 1024 * 1024;
                DEBUG("Optimize for 'raw' image on 'nfs': read_size=%ld",
                      opt->read_size);

                /* If user did not specify queue depth, adapt queue size to
                 * read size. */
                if ((opt->flags & USER_QUEUE_DEPTH) == 0) {
                    opt->queue_depth = 4;
                    DEBUG("Optimize for 'raw' image on 'nfs': queue_depth=%ld",
                          opt->queue_depth);
                }
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

static inline const char *command_name(struct command *cmd)
{
    return cmd->zero ? "Zero" : "Read";
}

static struct command *create_command(int64_t offset, uint32_t length, bool zero)
{
    struct command *c;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        FAIL_ERRNO("calloc");

    if (!zero) {
        c->buf = malloc(length);
        if (c->buf == NULL)
            FAIL_ERRNO("malloc");
    }

    c->offset = offset;
    c->length = length;
    c->completed = zero;
    c->zero = zero;

    if (debug)
        c->started = gettime();

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
        FAIL("Read offset=%" PRIu64 " length=%" PRIu32 " failed: %s",
             cmd->offset, cmd->length, strerror(*error));

    DEBUG("Read offset=%" PRIu64 " length=%" PRIu32 " completed in %" PRIu64
          " usec",
          cmd->offset, cmd->length, gettime() - cmd->started);

    cmd->completed = true;

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

static void start_read(struct worker *w, uint32_t length)
{
    struct command *cmd;

    cmd = create_command(w->read_offset, length, false);
    push_command(w, cmd);
    w->read_offset += cmd->length;

    src_aio_pread(w->s, cmd->buf, cmd->length, cmd->offset, read_completed, cmd);

    DEBUG("Read offset=%" PRIi64 " length=%" PRIu32 " started",
          cmd->offset, cmd->length);
}

static void start_zero(struct worker *w, uint32_t length)
{
    struct command *cmd;

    cmd = create_command(w->read_offset, length, true);
    push_command(w, cmd);
    w->read_offset += cmd->length;

    DEBUG("Zero offset=%" PRIi64 " length=%" PRIu32 " started",
          cmd->offset, cmd->length);
}

static void hash_more_data(struct worker *w)
{
    struct command *cmd = pop_command(w);

    assert(cmd->completed);

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

    w->bytes_hashed += cmd->length;
    if (w->opt->progress)
        progress_update(cmd->length);

    DEBUG("%s offset=%" PRIu64 " length=%" PRIu32 " finished in %" PRIu64
          " usec ",
          command_name(cmd), cmd->offset, cmd->length,
          gettime() - cmd->started);

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

static void fetch_extents(struct worker *w, uint32_t length)
{
    uint64_t start = 0;
    clear_extents(w);

    DEBUG("Get extents offset=%" PRIi64 " length=%" PRIu32,
          w->read_offset, length);

    if (debug)
        start = gettime();

    src_extents(w->s, w->read_offset, length, &w->extents.array,
                &w->extents.count);

    DEBUG("Got %lu extents in %" PRIu64 " usec",
          w->extents.count, gettime() - start);
}

static inline bool need_extents(struct worker *w)
{
    return w->extents.index == w->extents.count;
}

static void next_extent(struct worker *w, struct extent *extent)
{
    struct extent *current;

    if (need_extents(w)) {
        uint32_t length = MIN(w->opt->extents_size,
                              w->image_size - w->read_offset);
        fetch_extents(w, length);
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

static void read_more_data(struct worker *w)
{
    while (w->read_offset < w->image_size &&
           w->commands_in_flight < w->opt->queue_depth) {
        struct extent extent;

        next_extent(w, &extent);

        if (extent.zero)
            start_zero(w, extent.length);
        else
            start_read(w, extent.length);
    }
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
    while (w->bytes_hashed < w->image_size) {
        if (w->read_offset < w->image_size)
            read_more_data(w);

        if (wait_for_events(w))
            FAIL("Worker failed");

        while (has_data(w))
            hash_more_data(w);

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

    STAILQ_INIT(&w->read_queue);
    w->commands_in_flight = 0;

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
