// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>

#include "blkhash-internal.h"

void completion_init(struct completion *c, completion_callback cb,
                     void *user_data)
{
    c->callback = cb;
    c->user_data = user_data;
    c->refs = 1;
}

void completion_ref(struct completion *c)
{
    __atomic_add_fetch(&c->refs, 1, __ATOMIC_RELAXED);
}

void completion_unref(struct completion *c)
{
    if (__atomic_sub_fetch(&c->refs, 1, __ATOMIC_ACQ_REL) == 0)
        c->callback(c->user_data);
}
