// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BLKHASH_INTERNAL_H
#define BLKHASH_INTERNAL_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "blkhash-config.h"
#include "blkhash.h"

#define ABORTF(fmt, ...) do { \
    fprintf(stderr, "blkhash: " fmt "\n", ## __VA_ARGS__); \
    abort(); \
} while (0)

struct blkhash_opts {
    const char *digest_name;
    uint32_t block_size;
    unsigned queue_depth;
    uint8_t threads;
    uint8_t streams;
};

struct config {
    unsigned char zero_md[BLKHASH_MAX_MD_SIZE];
    const char *digest_name;
    uint32_t block_size;
    unsigned md_len;
    unsigned streams;
    unsigned workers;
    unsigned queue_depth;
    unsigned max_submissions;

    /* Align to avoid false sharing between workers. */
} __attribute__ ((aligned (CACHE_LINE_SIZE)));

typedef void (*completion_callback)(struct blkhash *h, void *user_data, int error);

struct completion {
    completion_callback callback;
    struct blkhash *hash;
    void *user_data;
    int error;
    unsigned refs;
};

int config_init(struct config *c, const struct blkhash_opts *opts);

struct completion *completion_new(completion_callback cb, struct blkhash *hash,
                                  void *user_data);
void completion_set_error(struct completion *c, int error);
void completion_ref(struct completion *c);
void completion_unref(struct completion *c);

bool is_zero(const void *buf, size_t len);

#endif /* BLKHASH_INTERNAL_H */
