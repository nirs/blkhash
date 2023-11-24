// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "digest.h"

static int null_init(struct digest *d)
{
    (void)d;
    return 0;
}

static int null_update(struct digest *d, const void *data, size_t len)
{
    (void)d;
    (void)data;
    (void)len;
    return 0;
}

static int null_final(struct digest *d, unsigned char *out, unsigned int *len)
{
    (void)d;
    (void)out;
    if (len)
        *len = 0; /* Empty digest */
    return 0;
}

static void null_destroy(struct digest *d)
{
    (void)d;
}

static struct digest_ops null_ops = {
    .init = null_init,
    .update = null_update,
    .finalize = null_final,
    .destroy = null_destroy,
};

static struct digest null_digest = {
    .ops = &null_ops
};

static int create_null(struct digest **out)
{
    *out = &null_digest;
    return 0;
}

int digest_list(const char **names, size_t *len)
{
    *names = calloc(1, sizeof(**names));
    if (*names == NULL)
        return -errno;

    names[0] = "null";
    *len = 1;

    return 0;
}

int digest_create(const char *name, struct digest **out)
{
    if (strcmp(name, "null") == 0)
        return create_null(out);

    *out = NULL;
    return -EINVAL;
}
