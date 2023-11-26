// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <openssl/evp.h>
#include <openssl/objects.h>

#include "digest.h"

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
}

size_t digest_list(const char **names, size_t len)
{
    struct digest_list d = {.names=names, .cap=len};

    add_builtin(&d);
    add_evp(&d);

    return d.len;
}
