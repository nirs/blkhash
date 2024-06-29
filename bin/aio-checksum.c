// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <unistd.h>

#include "blkhash-config.h"
#include "blkhash.h"
#include "blksum.h"
#include "util.h"
#include "src.h"

/* Limit the number of extents processed in one call. */
#define MAX_EXTENTS 4096

enum {HASH_FD=0, SRC_FD};

struct extent_array {
    struct extent *array;
    size_t count;
    size_t index;
};

struct command {
    STAILQ_ENTRY(command) read_entry;
    TAILQ_ENTRY(command) hash_entry;
    struct worker *w;
    void *buf;
    int64_t offset;
    uint64_t started;
    uint32_t length;
    bool completed;
    bool zero;
};

struct worker {
    pthread_t thread;

    /* For polling source I/O events and hash completion events. */
    struct pollfd poll_fds[2];

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
    TAILQ_HEAD(, command) hash_queue;
    unsigned commands_in_flight;

    /* The computed checksum. */
    unsigned char *out;
    unsigned int *len;
};

static struct command *create_command(struct worker *w)
{
    struct command *c;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        FAIL_ERRNO("calloc");

    c->buf = malloc(w->opt->read_size);
    if (c->buf == NULL)
        FAIL_ERRNO("malloc");

    c->w = w;

    return c;
}

static void init_command(struct command *c, int64_t offset, uint32_t length, bool zero)
{
    c->offset = offset;
    c->length = length;
    c->zero = zero;
    c->completed = zero;

    if (debug)
        c->started = gettime();
}

static void free_command(struct command *c)
{
    if (c) {
        free(c->buf);
        free(c);
    }
}

static void start_update(struct worker *w, struct command *cmd)
{
    int err;

    cmd->completed = false;

    if (debug)
        cmd->started = gettime();

    err = blkhash_aio_update(w->h, cmd->buf, cmd->length, cmd);
    if (err)
        FAIL("blkhash_aio_update: %s", strerror(err));

    DEBUG("Update offset=%" PRIu64 " length=%" PRIu32 " started",
          cmd->offset, cmd->length);

    assert(w->commands_in_flight < w->opt->queue_depth);
    w->commands_in_flight++;
}

static void update_completed(struct worker *w, struct command *cmd)
{
    DEBUG("Update offset=%" PRIi64 " length=%" PRIu32 " completed in %" PRIu64
          " usec",
          cmd->offset, cmd->length, gettime() - cmd->started);

    cmd->completed = true;

    w->bytes_hashed += cmd->length;

    assert(w->commands_in_flight > 0);
    w->commands_in_flight--;

    if (w->opt->progress)
        progress_update(cmd->length);
}

static void run_zero(struct worker *w, struct command *cmd)
{
    int err;

    if (debug)
        cmd->started = gettime();

    err = blkhash_zero(w->h, cmd->length);
    if (err)
        FAIL("blkhash_zero: %s", strerror(err));

    DEBUG("Zero offset=%" PRIi64 " length=%" PRIu32 " completed in %" PRIu64
          " usec",
          cmd->offset, cmd->length, gettime() - cmd->started);

    cmd->completed = true;

    w->bytes_hashed += cmd->length;

    if (w->opt->progress)
        progress_update(cmd->length);
}

static void hash_more_data(struct worker *w)
{
    struct command *cmd, *next;

    cmd = STAILQ_FIRST(&w->read_queue);

    while (cmd != NULL && cmd->completed) {
        next = STAILQ_NEXT(cmd, read_entry);

        STAILQ_REMOVE_HEAD(&w->read_queue, read_entry);
        TAILQ_INSERT_TAIL(&w->hash_queue, cmd, hash_entry);

        if (cmd->zero)
            run_zero(w, cmd);
        else
            start_update(w, cmd);

        cmd = next;
    }
}

static void fetch_extents(struct worker *w, uint32_t length)
{
    uint64_t start = 0;

    w->extents.index = 0;
    w->extents.count = MAX_EXTENTS;

    DEBUG("Get extents offset=%" PRIi64 " length=%" PRIu32,
          w->read_offset, length);

    if (debug)
        start = gettime();

    src_extents(w->s, w->read_offset, length, w->extents.array,
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

    assert(cmd->w->commands_in_flight > 0);
    cmd->w->commands_in_flight--;

    /* Required for linbd to "retire" the command. */
    return 1;
}

static void start_read(struct worker *w, struct command *cmd)
{
    /* Must increase the conter before calling, since sources faking aync
     * support will call read_completed *before* the call returns. */
    assert(w->commands_in_flight < w->opt->queue_depth);
    w->commands_in_flight++;

    DEBUG("Read offset=%" PRIi64 " length=%" PRIu32 " started",
          cmd->offset, cmd->length);

    src_aio_pread(w->s, cmd->buf, cmd->length, cmd->offset, read_completed, cmd);
}

static void start_zero(struct worker *w __attribute__ ((unused)), struct command *cmd)
{
    DEBUG("Zero offset=%" PRIi64 " length=%" PRIu32 " started",
          cmd->offset, cmd->length);
}

static void read_more_data(struct worker *w)
{
    struct command *cmd, *next;

    cmd = TAILQ_FIRST(&w->hash_queue);

    while (cmd != NULL && w->read_offset < w->image_size) {
        next = TAILQ_NEXT(cmd, hash_entry);

        if (cmd->completed) {
            struct extent extent;

            TAILQ_REMOVE(&w->hash_queue, cmd, hash_entry);
            STAILQ_INSERT_TAIL(&w->read_queue, cmd, read_entry);

            next_extent(w, &extent);
            init_command(cmd, w->read_offset, extent.length, extent.zero);

            if (cmd->zero)
                start_zero(w, cmd);
            else
                start_read(w, cmd);

            w->read_offset += cmd->length;
        }

        cmd = next;
    }
}

static int complete_updates(struct worker *w)
{
    struct blkhash_completion completions[w->opt->queue_depth];
    int n;

    n = blkhash_aio_completions(w->h, completions, w->opt->queue_depth);
    if (n < 0) {
        ERROR("Failed to get update completions: %s\n", strerror(-n));
        return -1;
    }

    DEBUG("Got %d updates completions", n);

    for (int i = 0; i < n; i++) {
        struct blkhash_completion *c = &completions[i];
        struct command *cmd = c->user_data;

        if (c->error) {
            ERROR("Update offset=%" PRIi64 " length=%" PRIu32 " failed: %s\n",
                  cmd->offset, cmd->length, strerror(c->error));
            return -1;
        }

        update_completed(w, cmd);
    }

    return 0;
}

static int wait_for_events(struct worker *w)
{
    int n;

    if (src_aio_prepare(w->s, &w->poll_fds[SRC_FD]))
        return -1;

    do {
        n = poll(w->poll_fds, 2, -1);
    } while (n == -1 && errno == EINTR);

    if (n == -1) {
        ERROR("Polling failed: %s", strerror(errno));
        return -1;
    }

    if (w->poll_fds[SRC_FD].revents) {
        if (src_aio_notify(w->s, &w->poll_fds[SRC_FD]))
            return -1;
    }

    if (w->poll_fds[HASH_FD].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        ERROR("Error on completion fd %d revents=%d\n",
              w->poll_fds[HASH_FD].fd, w->poll_fds[HASH_FD].revents);
        return -1;
    }

    if (w->poll_fds[HASH_FD].revents & POLLIN) {
        /* We must read at least 8 bytes from the completion fd. */
        char sink[w->opt->queue_depth < 8 ? 8 : w->opt->queue_depth];

        do {
            n = read(w->poll_fds[HASH_FD].fd, &sink, sizeof(sink));
        } while (n == -1 && errno == EINTR);

        if (n < 0) {
            ERROR("read: %s", strerror(errno));
            return -1;
        }

        if (n > 0) {
            if (complete_updates(w))
                return -1;
        }
    }

    return 0;
}

static void process_image(struct worker *w)
{
    while (w->bytes_hashed < w->image_size) {
        if (w->read_offset < w->image_size)
            read_more_data(w);

        hash_more_data(w);

        if (w->commands_in_flight) {
            if (wait_for_events(w))
                FAIL("Worker failed");
        }

        if (!running()) {
            DEBUG("Worker aborting");
            break;
        }
    }
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
        err = blkhash_final(w->h, w->out, w->len);

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
    int fd;

    ho = blkhash_opts_new(w->opt->digest_name);
    if (ho == NULL)
        FAIL_ERRNO("blkhash_opts_new");

    if (blkhash_opts_set_queue_depth(ho, w->opt->queue_depth))
        FAIL("Invalid queue depth value: %u", w->opt->queue_depth);

    if (blkhash_opts_set_block_size(ho, w->opt->block_size))
        FAIL("Invalid block size value: %zu", w->opt->block_size);

    if (blkhash_opts_set_threads(ho, w->opt->threads))
        FAIL("Invalid threads value: %zu", w->opt->threads);

    w->h = blkhash_new_opts(ho);
    blkhash_opts_free(ho);
    if (w->h == NULL)
        FAIL_ERRNO("blkhash_new");

    fd = blkhash_aio_completion_fd(w->h);
    if (fd < 0)
        FAIL("blkhash_aio_completion_fd: %s", strerror(-fd));

    w->poll_fds[HASH_FD].fd = fd;
    w->poll_fds[HASH_FD].events = POLLIN;
}

#ifdef HAVE_NBD
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
#endif

static void init_worker(struct worker *w, const char *filename, struct options
                        *opt, unsigned char *out, unsigned int *len)
{
    w->opt = opt;
    w->out = out;
    w->len = len;

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

    w->extents.array = malloc(MAX_EXTENTS * sizeof(*w->extents.array));
    if (w->extents.array == NULL)
        FAIL_ERRNO("malloc");

    STAILQ_INIT(&w->read_queue);
    TAILQ_INIT(&w->hash_queue);

    /* Fill the hash queue with "completed" commands. The process loop will
     * move them to the read queue. */
    for (size_t i = 0; i < w->opt->queue_depth; i++) {
        struct command *cmd = create_command(w);
        cmd->completed = true;
        TAILQ_INSERT_TAIL(&w->hash_queue, cmd, hash_entry);
    }

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

static void destroy_commands(struct worker *w)
{
    struct command *cmd, *next;

    cmd = STAILQ_FIRST(&w->read_queue);
    while (cmd != NULL) {
        next = STAILQ_NEXT(cmd, read_entry);
        STAILQ_REMOVE_HEAD(&w->read_queue, read_entry);
        free_command(cmd);
        cmd = next;
    }

    cmd = TAILQ_FIRST(&w->hash_queue);
    while (cmd != NULL) {
        next = TAILQ_NEXT(cmd, hash_entry);
        TAILQ_REMOVE(&w->hash_queue, cmd, hash_entry);
        free_command(cmd);
        cmd = next;
    }
}

static void destroy_worker(struct worker *w)
{
    blkhash_free(w->h);
    free(w->uri);
    free(w->extents.array);
    destroy_commands(w);

#ifdef HAVE_NBD
    if (w->nbd_server) {
        stop_nbd_server(w->nbd_server);
        free(w->nbd_server);
    }
#endif
}

void aio_checksum(const char *filename, struct options *opt,
                  unsigned char *out, unsigned int *len)
{
    struct worker w = {0};

    init_worker(&w, filename, opt, out, len);
    start_worker(&w);
    join_worker(&w);
    destroy_worker(&w);
}
