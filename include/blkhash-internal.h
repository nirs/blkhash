// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKHASH_INTERNAL_H
#define BLKHASH_INTERNAL_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include <openssl/evp.h>

#include "blkhash-config.h"

#define ABORTF(fmt, ...) do { \
    fprintf(stderr, "blkhash: " fmt "\n", ## __VA_ARGS__); \
    abort(); \
} while (0)

struct blkhash_opts {
    const char *digest_name;
    size_t block_size;
    uint8_t threads;
    uint8_t streams;
};

struct config {
    unsigned char zero_md[EVP_MAX_MD_SIZE];
    const EVP_MD *md;
    size_t block_size;
    unsigned int md_len;
    unsigned streams;
    unsigned workers;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

struct stream {
    const struct config *config;

    /* The stream hash context consuming block digests. */
    EVP_MD_CTX *root_ctx;

    /* For computing block digest. */
    EVP_MD_CTX *block_ctx;

    /* Last consumed block index. */
    int64_t last_index;

    /* Used to fill in zero blocks. */
    int id;

    /* If non-zero, the stream has failed. The value is the first error that
     * caused the failure. */
    int error;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

struct block {
    /* Entry in the worker queue handling this block stream. */
    STAILQ_ENTRY(block) entry;

    /* The stream handling this block. */
    struct stream *stream;

    int64_t index;
    size_t len;

    /* If true, the worker should terminate. */
    bool last;

    /* If len > 0, the block data. */
    unsigned char data[0];
};

struct worker {
    STAILQ_HEAD(, block) queue;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    unsigned int queue_len;

    /* If non-zero, the worker has failed. The value is the first error that
     * caused the failure. */
    int error;

    /* Set to false when worker is stopped by the final zero length block, or
     * when a worker fails. */
    bool running;

    /* Set when stopping the worker. No updates are allowed after this. */
    bool stopped;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

int config_init(struct config *c, const struct blkhash_opts *opts);

struct block *block_new(struct stream *stream, uint64_t index, size_t len,
                        const void *data);
void block_free(struct block *b);

int stream_init(struct stream *s, int id, const struct config *config);
int stream_update(struct stream *s, const struct block *b);
int stream_final(struct stream *s, unsigned char *md, unsigned int *len);
void stream_destroy(struct stream *s);

int worker_init(struct worker *w);
int worker_submit(struct worker *w, struct block *b);
int worker_stop(struct worker *w);
int worker_join(struct worker *w);
void worker_destroy(struct worker *w);

bool is_zero(const void *buf, size_t len);

#endif /* BLKHASH_INTERNAL_H */
