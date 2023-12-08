// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "blkhash-config.h"
#include "digest.h"

#ifdef HAVE_BLAKE3

#include <blake3.h>

struct blake3_digest {
    struct digest digest;
    blake3_hasher hasher;
};

static int blake3_ops_init(struct digest *d)
{
    struct blake3_digest *b = (struct blake3_digest *)d;
    blake3_hasher_init(&b->hasher);
    return 0;
}

static int blake3_ops_update(struct digest *d, const void *data, size_t len)
{
    struct blake3_digest *b = (struct blake3_digest *)d;
    blake3_hasher_update(&b->hasher, data, len);
    return 0;
}

static int blake3_ops_final(struct digest *d, unsigned char *out, unsigned int *len)
{
    struct blake3_digest *b = (struct blake3_digest *)d;
    blake3_hasher_finalize(&b->hasher, out, BLAKE3_OUT_LEN);
    if (len)
        *len = BLAKE3_OUT_LEN;
    return 0;
}

static void blake3_ops_destroy(struct digest *d)
{
    free(d);
}

static struct digest_ops blake3_ops = {
    .init = blake3_ops_init,
    .update = blake3_ops_update,
    .finalize = blake3_ops_final,
    .destroy = blake3_ops_destroy,
};

static int create_blake3(struct digest **out)
{
    struct blake3_digest *b;

    b = calloc(1, sizeof(*b));
    if (b == NULL)
        return -errno;

    b->digest.ops = &blake3_ops;
    *out = &b->digest;
    return 0;
}

#endif

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

#ifdef HAVE_BLAKE3
    if (strcmp(name, "blake3") == 0)
        return create_blake3(out);
#endif

    return create_evp(name, out);
}

struct digest_list {
    const char **names;
    size_t len;
    size_t cap;
};

static void append(struct digest_list *d, const char *name)
{
    if (d->cap && d->len == d->cap)
        return;

    /* If names is NULL, we only count the names. */
    if (d->names)
        d->names[d->len] = name;

    d->len++;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static void append_evp(EVP_MD *md, void *arg)
#else
static void append_evp(const EVP_MD *md,
                          const char *from __attribute__ ((unused)),
                          const char *to __attribute__ ((unused)),
                          void *arg)
#endif
{
    struct digest_list *digests = arg;
    int nid;
    const char *name;

    if (md == NULL)
        return;

    nid = EVP_MD_nid(md);
    if (nid == NID_undef)
        return;

    name = OBJ_nid2ln(nid);
    if (name == NULL)
        name = OBJ_nid2sn(nid);

    append(digests, name);
}

static void add_evp(struct digest_list *d)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    /* Get digests from active providers in default context. */
    EVP_MD_do_all_provided(NULL, append_evp, d);
#else
    EVP_MD_do_all(append_evp, d);
#endif
}

static void add_builtin(struct digest_list *d)
{
    append(d, "null");
#ifdef HAVE_BLAKE3
    append(d, "blake3");
#endif
}

size_t blkhash_digests(const char **names, size_t len)
{
    struct digest_list d = {.names=names, .cap=len};

    add_builtin(&d);
    add_evp(&d);

    return d.len;
}
