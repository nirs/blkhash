// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>

#include "blkhash-internal.h"

struct completion *completion_new(blkhash_callback cb, void *user_data)
{
    struct completion *c;

    c = malloc(sizeof(*c));
    if (c == NULL)
        return NULL;

    c->callback = cb;
    c->user_data = user_data;
    c->refs = 1;
    c->error = 0;

    return c;
}

void completion_set_error(struct completion *c, int error)
{
    /* Store the first error atomically. */
    int expected = 0;
    __atomic_compare_exchange_n(&c->error, &expected, error, false,
                                __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

void completion_ref(struct completion *c)
{
    __atomic_add_fetch(&c->refs, 1, __ATOMIC_RELAXED);
}

void completion_unref(struct completion *c)
{
    if (__atomic_sub_fetch(&c->refs, 1, __ATOMIC_ACQ_REL) == 0) {
        int error = __atomic_load_n(&c->error, __ATOMIC_ACQUIRE);
        c->callback(c->user_data, error);
        free(c);
    }
}
