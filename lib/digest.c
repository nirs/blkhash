// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

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

struct evp_digest {
    struct digest digest;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD *md;
#else
    const EVP_MD *md;
#endif
    EVP_MD_CTX *ctx;
};

static int evp_init(struct digest *d)
{
    struct evp_digest *evp = (struct evp_digest *)d;

    if (!EVP_DigestInit_ex(evp->ctx, evp->md, NULL))
        return -ENOMEM;

    return 0;
}

static int evp_update(struct digest *d, const void *data, size_t len)
{
    struct evp_digest *evp = (struct evp_digest *)d;

    if (!EVP_DigestUpdate(evp->ctx, data, len))
        return -ENOMEM;

    return 0;
}

static int evp_final(struct digest *d, unsigned char *md, unsigned int *len)
{
    struct evp_digest *evp = (struct evp_digest *)d;

    if (!EVP_DigestFinal_ex(evp->ctx, md, len))
        return -ENOMEM;

    return 0;
}

static void evp_destroy(struct digest *d)
{
    struct evp_digest *evp = (struct evp_digest *)d;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_MD_free(evp->md);
#endif
    EVP_MD_CTX_free(evp->ctx);
    free(evp);
}

struct digest_ops evp_ops = {
    .init = evp_init,
    .update = evp_update,
    .finalize = evp_final,
    .destroy = evp_destroy,
};

static int create_evp(const char *name, struct digest **out)
{
    struct evp_digest *evp;
    int err = 0;

    evp = calloc(1, sizeof(*evp));
    if (evp == NULL)
        return -errno;

    evp->digest.ops = &evp_ops;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    evp->md = EVP_MD_fetch(NULL, name, NULL);
#else
    evp->md = EVP_get_digestbyname(name);
#endif
    if (evp->md == NULL) {
        err = -EINVAL;
        goto error;
    }

    evp->ctx = EVP_MD_CTX_new();
    if (evp->ctx == NULL) {
        err = -ENOMEM;
        goto  error;
    }

    *out = &evp->digest;
    return 0;

error:
    evp_destroy(&evp->digest);
    return err;
}

int digest_create(const char *name, struct digest **out)
{
    if (strcmp(name, "null") == 0)
        return create_null(out);

    return create_evp(name, out);
}
