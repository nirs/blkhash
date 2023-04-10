// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

/* blkhash example using the async interface. */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <blkhash.h>
#include <blkhash-internal.h>

#define KiB (1ULL << 10)
#define MiB (1ULL << 20)
#define GiB (1ULL << 30)

#define TOTAL_SIZE (50 * MiB)
#define BUF_SIZE (256 * KiB)
#define QUEUE_DEPTH 16
#define DIGEST "sha256"
#define THREADS 16

struct command {
    void *buffer;
    int error;
    bool ready;
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int in_flight;

/*
 * blkhash callback, invoked from a blkhsh worker thread when update completes.
 */
void update_completed(void *user_data, int error)
{
    struct command *cmd = user_data;

    pthread_mutex_lock(&mutex);

    cmd->error = error;
    cmd->ready = true;
    in_flight--;
    pthread_cond_signal(&cond);

    pthread_mutex_unlock(&mutex);
}

int main()
{
    struct blkhash_opts *opts = NULL;
    struct blkhash *h = NULL;
    struct command *commands = NULL;
    uint64_t updated_size = 0;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    unsigned int md_len;
    int err;

    opts = blkhash_opts_new(DIGEST);
    if (opts == NULL) {
        err = errno;
        goto out;
    }

    err = blkhash_opts_set_streams(opts, 64);
    if (err)
        goto out;

    err = blkhash_opts_set_threads(opts, THREADS);
    if (err)
        goto out;

    h = blkhash_new_opts(opts);
    if (h == NULL) {
        err = errno;
        goto out;
    }

    commands = calloc(QUEUE_DEPTH, sizeof(*commands));
    if (commands == NULL) {
        err = errno;
        goto out;
    }

    for (int i = 0; i < QUEUE_DEPTH; i++) {
        struct command *cmd = &commands[i];

        err = posix_memalign(&cmd->buffer, CACHE_LINE_SIZE, BUF_SIZE);
        if (err)
            goto out;

        memset(cmd->buffer, 'A' + i, BUF_SIZE);
        cmd->ready = true;
    }

    while (updated_size < TOTAL_SIZE) {
        /*
         * Update ready buffers.
         */

        for (int i = 0; i < QUEUE_DEPTH && updated_size < TOTAL_SIZE; i++) {
            struct command *cmd = &commands[i];
            int error;
            bool ready;

            pthread_mutex_lock(&mutex);
            error = cmd->error;
            ready = cmd->ready;
	    if (ready && !error) {
                cmd->ready = false;
		in_flight++;
	    }
            pthread_mutex_unlock(&mutex);

            if (error) {
                err = error;
                goto out;
            }

            if (ready) {
                err = blkhash_async_update(h, cmd->buffer, BUF_SIZE,
                                           update_completed, cmd);
                if (err)
                    goto out;

                updated_size += BUF_SIZE;
            }
        }

        /*
         * Wait for completions.
         */

	pthread_mutex_lock(&mutex);

	/* Wait until at least one command finish. */
	while (in_flight == QUEUE_DEPTH)
            pthread_cond_wait(&cond, &mutex);

	pthread_mutex_unlock(&mutex);
    }

    /* Finalize the hash and print a message digest. */
    err = blkhash_final(h, md, &md_len);
    if (err)
        goto out;

    for (unsigned i = 0; i < md_len; i++)
        printf("%02x", md[i]);

    printf("\n");

out:
    blkhash_free(h);
    blkhash_opts_free(opts);

    for (int i = 0; i < QUEUE_DEPTH; i++) {
        struct command *cmd = &commands[i];
        free(cmd->buffer);
    }

    free(commands);

    if (err) {
        fprintf(stderr, "Example failed: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    return 0;
}
