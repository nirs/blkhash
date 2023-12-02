// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef DIGEST_H
#define DIGEST_H

#include <stdint.h>
#include <unistd.h>

struct digest {
    struct digest_ops *ops;
};

struct digest_ops {
    int (*init)(struct digest *d);
    int (*update)(struct digest *d, const void *data, size_t len);
    int (*finalize)(struct digest *d, unsigned char *out, unsigned int *len);
    void (*destroy)(struct digest *d);
};

/* Lookup name and create a new digest. */
int digest_create(const char *name, struct digest **out);

/* Initalize a digest for computing a new hash. */
static inline int digest_init(struct digest *d)
{
    return d->ops->init(d);
}

/* Add data to the digest. */
static inline int digest_update(struct digest *d, const void *data, size_t len)
{
    return d->ops->update(d, data, len);
}

/* Finalize the digest and return the digest and length. */
static inline int digest_final(struct digest *d, unsigned char *out, unsigned int *len)
{
    return d->ops->finalize(d, out, len);
}

/* Free resources allocated by digest_create(). */
static inline void digest_destroy(struct digest *d)
{
    /* Keep free() semantics. */
    if (d)
        d->ops->destroy(d);
}

#endif /* DIGEST_H */
