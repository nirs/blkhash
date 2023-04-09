// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

/* blkhash example using the async interface. */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <blkhash.h>

#define MiB (1ULL << 20)
#define TOTAL_SIZE (50 * MiB)
#define BUF_SIZE (256 * 1024)
#define QUEUE_DEPTH 16

struct command {
    unsigned char *buffer;
    int error;
    bool ready;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/*
 * blkhash callback, invoked from a blkhsh worker thread when update completes.
 */
void update_completed(void *user_data, int error)
{
    struct command *cmd = user_data;

    pthread_mutex_lock(&mutex);
    cmd->error = error;
    cmd->ready = true;
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

    opts = blkhash_opts_new("sha256");
    if (opts == NULL) {
        err = errno;
        goto out;
    }

    err = blkhash_opts_set_threads(opts, QUEUE_DEPTH);
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

        cmd->buffer = malloc(BUF_SIZE);
        if (cmd->buffer == NULL)
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
