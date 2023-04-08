// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef THREADS_H
#define THREADS_H

#include <string.h>
#include "blkhash-internal.h"

static inline void mutex_lock(pthread_mutex_t *m)
{
    int err = pthread_mutex_lock(m);
    if (err)
        ABORTF("pthread_mutex_lock: %s", strerror(err));
}

static inline void mutex_unlock(pthread_mutex_t *m)
{
    int err = pthread_mutex_unlock(m);
    if (err)
        ABORTF("pthread_mutex_unlock: %s", strerror(err));
}

static inline void mutex_destroy(pthread_mutex_t *m)
{
    int err = pthread_mutex_destroy(m);
    if (err)
        ABORTF("pthread_mutex_destroy: %s", strerror(err));
}

static inline void cond_signal(pthread_cond_t *c)
{
    int err = pthread_cond_signal(c);
    if (err)
        ABORTF("pthread_cond_signal: %s", strerror(err));
}

static inline void cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    int err = pthread_cond_wait(c, m);
    if (err)
        ABORTF("pthread_cond_wait: %s", strerror(err));
}

static inline void cond_destroy(pthread_cond_t *c)
{
    int err = pthread_cond_destroy(c);
    if (err)
        ABORTF("pthread_cond_destroy: %s", strerror(err));
}

#endif /* THREADS_H */
