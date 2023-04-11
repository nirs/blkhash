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
    const char *digest_name;
    size_t block_size;
    unsigned int md_len;
    unsigned streams;
    unsigned workers;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

struct stream {
    const struct config *config;

    /* Used to intialize root_ctx and block_ctx. */
    const EVP_MD *md;

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

typedef void (*completion_callback)(void *user_data);

struct completion {
    completion_callback callback;
    void *user_data;
    unsigned refs;
};

enum submission_type {DATA, ZERO, STOP};

struct submission {
    enum submission_type type;

    /* Entry in the worker queue handling this submission stream. */
    STAILQ_ENTRY(submission) entry;

    /* The stream handling this submission. */
    struct stream *stream;

    /* Completion for DATA submission, used to wait until all submissions are
     * handled by the workers. */
    struct completion *completion;

    int64_t index;

    /* Length of data for DATA submission. */
    size_t len;

    /* Data for DATA submission. */
    unsigned char data[0];
};

struct worker {
    STAILQ_HEAD(, submission) queue;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    unsigned int queue_len;

    /* If non-zero, the worker has failed. The value is the first error that
     * caused the failure. */
    int error;

    /* Set to false when worker is stopped by the final zero length submission, or
     * when a worker fails. */
    bool running;

    /* Set when stopping the worker. No updates are allowed after this. */
    bool stopped;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

int config_init(struct config *c, const struct blkhash_opts *opts);

void completion_init(struct completion *c, completion_callback cb,
                     void *user_data);
void completion_ref(struct completion *c);
void completion_unref(struct completion *c);

struct submission *submission_new_data(struct stream *stream, int64_t index,
                                       size_t len, const void *data,
                                       struct completion *completion);
struct submission *submission_new_zero(struct stream *stream, int64_t index);
struct submission *submission_new_stop(void);
void submission_free(struct submission *sub);

int stream_init(struct stream *s, int id, const struct config *config);
int stream_update(struct stream *s, const struct submission *sub);
int stream_final(struct stream *s, unsigned char *md, unsigned int *len);
void stream_destroy(struct stream *s);

int worker_init(struct worker *w);
int worker_submit(struct worker *w, struct submission *sub);
int worker_stop(struct worker *w);
int worker_join(struct worker *w);
void worker_destroy(struct worker *w);

bool is_zero(const void *buf, size_t len);

#endif /* BLKHASH_INTERNAL_H */
