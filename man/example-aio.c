// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

/* blkhash example using the async interface. */

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <blkhash.h>

#define KiB (1ULL << 10)
#define MiB (1ULL << 20)

#define IMAGE_SIZE (20 * MiB)
#define BUF_SIZE (256 * KiB)
#define QUEUE_DEPTH 16
#define DIGEST "sha256"
#define THREADS 16

struct request {
    unsigned char *buffer;
    uint64_t started;
    size_t len;
    int id;
    bool ready;
};

/* For waiting on blkhash async update compeletion. */
static struct pollfd poll_fds[1];

/* blkhash instance */
static struct blkhash *hash;

/* Keeps data for hashing. */
static struct request requests[QUEUE_DEPTH];

/* Current request index in requests array. */
static unsigned current;

/* Number of bytes hashed. */
static uint64_t updated_size;

static uint64_t gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void setup_requests(void)
{
    /* Initialize requests array. */

    for (int i = 0; i < QUEUE_DEPTH; i++) {
        struct request *req = &requests[i];

        req->buffer = malloc(BUF_SIZE);
        if (req->buffer == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        /*
         * Simulate a 50% full image, half of the blocks are all zeros. blkhash
         * optimizes hashing of zeroed areas.
         */
        memset(req->buffer, (i < QUEUE_DEPTH / 2) ? 'A' + i : 0, BUF_SIZE);

        req->len = BUF_SIZE;
        req->id = i;
        req->ready = true;
    }
}

static void create_hash(void)
{
    struct blkhash_opts *opts;
    int fd;
    int err;

    /* Create blkhash options for setting threads and event fd. */

    opts = blkhash_opts_new(DIGEST);
    if (opts == NULL) {
        perror("blkhash_opts_new");
        exit(EXIT_FAILURE);
    }

    /* Enable the async API. */
    err = blkhash_opts_set_queue_depth(opts, QUEUE_DEPTH);
    if (err) {
        fprintf(stderr, "blkhash_opts_set_queue_depth: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    err = blkhash_opts_set_threads(opts, THREADS);
    if (err) {
        fprintf(stderr, "blkhash_opts_set_threads: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    /* Crate blkhash with the options. */

    hash = blkhash_new_opts(opts);
    if (hash == NULL) {
        perror("blkhash_new_opts");
        exit(EXIT_FAILURE);
    }

    blkhash_opts_free(opts);

    fd = blkhash_aio_completion_fd(hash);
    if (fd < 0) {
        fprintf(stderr, "Cannot get completion fd: %s", strerror(-fd));
        exit(EXIT_FAILURE);
    }

    /* We need to poll the completion fd until it becomes readable. */
    poll_fds[0].fd = fd;
    poll_fds[0].events = POLLIN;
}

static void teardown(void)
{
    blkhash_free(hash);

    for (int i = 0; i < QUEUE_DEPTH; i++) {
        struct request *req = &requests[i];
        free(req->buffer);
    }
}

static int start_aio_update(struct request *req)
{
    int err;

    printf("Request %d starting\n", current);
    err = blkhash_aio_update(hash, req->buffer, req->len, req);
    if (err) {
        fprintf(stderr, "blkhash_aio_update: %s", strerror(err));
        return -1;
    }

    /* We cannot use this request before it completes. */
    req->ready = false;

    req->started = gettime();
    updated_size += req->len;

    return 0;
}

static int process_completions(void)
{
    struct blkhash_completion completions[QUEUE_DEPTH];
    int count;

    /* Get completions. */
    count = blkhash_aio_completions(hash, completions, QUEUE_DEPTH);
    if (count < 0) {
        printf("blkhash_aio_completions: %s\n", strerror(-count));
        return -1;
    }

    printf("Got %d completions\n", count);

    for (int i = 0; i < count; i++) {
        struct request *req = completions[i].user_data;

        /* Did request fail? */
        if (completions[i].error) {
            printf("Request %d failed: %s\n",
                   req->id, strerror(completions[i].error));
            return -1;
        }

        /* We can use this commnad again. */
        printf("Request %d completed in %" PRIu64 " usec\n",
               req->id, gettime() - req->started);
        req->ready = true;
    }

    return 0;
}

/*
 * Wait for blkhash completion events.  In a real application you would also
 * wait for read events from the source image.
 */
static int wait_for_events(void)
{
    for (;;) {
        printf("Waiting for events...\n");

        if (poll(poll_fds, 1, -1) == -1) {
            fprintf(stderr, "poll: %s", strerror(errno));
            return -1;
        }

        if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "Unexpected completion fd events: %d",
                    poll_fds[0].revents);
            return -1;
        }

        if (poll_fds[0].revents & POLLIN) {
            /* We must read at least 8 bytes from the completion fd. */
            char sink[QUEUE_DEPTH < 8 ? 8 : QUEUE_DEPTH];

            if (read(poll_fds[0].fd, sink, sizeof(sink)) == -1) {
                perror("read");
                return -1;
            }

            /* At least one request should be ready now. */
            return process_completions();
        }
    }
}

static void print_hash(void)
{
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    unsigned int md_len;
    int err;

    err = blkhash_final(hash, md, &md_len);
    if (err) {
        fprintf(stderr, "blkhash_final: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    for (unsigned i = 0; i < md_len; i++)
        printf("%02x", md[i]);

    printf("\n");
}

int main()
{
    struct request *req;
    int err;

    create_hash();
    setup_requests();

    req = &requests[0];

    while (updated_size < IMAGE_SIZE) {

        /* Hash ready requests buffers in order. */
        while (req->ready && updated_size < IMAGE_SIZE) {
            err = start_aio_update(req);
            if (err)
                goto out;

            current = (current + 1) % QUEUE_DEPTH;
            req = &requests[current];
        }

        /* Wait until current rquest is ready. */
        while (!req->ready) {
            err = wait_for_events();
            if (err)
                goto out;
        }
    }

    print_hash();

out:
    teardown();
    return err ? 1 : 0;
}
