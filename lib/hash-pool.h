// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef HASH_POOL_H
#define HASH_POOL_H

#include <pthread.h>
#include <stdbool.h>

#include "blkhash-config.h"

struct hash_pool {
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    pthread_mutex_t mutex;
    pthread_t *workers;
    struct submission **queue;
    const struct config *config;

    unsigned int queue_size;

    /* XXX replace with computed length:
     * (size + tail - head) % size
     * assuming empty slot (size - 1 items)
     */
    unsigned int queue_len;

    unsigned int queue_head;
    unsigned int queue_tail;

    unsigned int workers_count;
    unsigned int workers_idle;

    int error;
    bool stopped;

} __attribute__ ((aligned (CACHE_LINE_SIZE)));

int hash_pool_init(struct hash_pool *p, const struct config *config);

int hash_pool_submit(struct hash_pool *p, struct submission *sub);

int hash_pool_destroy(struct hash_pool *p);

#endif /* HASH_POOL_H */
