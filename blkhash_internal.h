// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKHASH_INTERNAL_H
#define BLKHASH_INTERNAL_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#include <openssl/evp.h>

struct config {
    size_t block_size;
    int workers;
    const EVP_MD *md;
    unsigned char zero_md[EVP_MAX_MD_SIZE];
    unsigned int zero_md_len;
};

typedef void (*complete_callback)(int64_t index);

struct block {
    STAILQ_ENTRY(block) entry;
    int64_t index;
    size_t len;
    unsigned char data[0];
};

struct worker {
    STAILQ_HEAD(, block) queue;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    struct config *config;
    int id;
    unsigned int queue_len;

    EVP_MD_CTX *block_ctx;
    EVP_MD_CTX *root_ctx;

    /* Last consumed block. */
    int64_t last_index;
};

struct config *config_new(const char *digest_name, size_t block_size, int workers);
void config_free(struct config *c);

struct block *block_new(uint64_t index, size_t len, const void *data);
void block_free(struct block *b);

int worker_init(struct worker *w, int id, struct config *config);
int worker_destroy(struct worker *w);
int worker_update(struct worker *w, struct block *b);
int worker_final(struct worker *w, int64_t size);
int worker_digest(struct worker *w, unsigned char *md, unsigned int *len);

bool is_zero(const void *buf, size_t len);

#endif /* BLKHASH_INTERNAL_H */
