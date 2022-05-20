// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/evp.h>
#include <openssl/objects.h>

#include "blksum.h"

struct digests {
    const char **names;
    size_t len;
    size_t cap;
};

static void grow(struct digests *d)
{
    size_t cap = d->len * 2;
    void *names;

    names = realloc(d->names, cap * sizeof(*d->names));
    if (names == NULL)
        FAIL_ERRNO("realloc");

    d->names = names;
    d->cap = cap;
}

static void append(struct digests *d, const char *name)
{
    if (d->len == d->cap)
        grow(d);

    d->names[d->len] = name;
    d->len++;
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static void append_digest(EVP_MD *md, void *arg)
#else
static void append_digest(const EVP_MD *md, const char *from, const char *to,
                         void *arg)
#endif
{
    struct digests *digests = arg;
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

static int compare(const void *p1, const void *p2)
{
   return strcmp(*(const char **)p1, *(const char **)p2);
}

void list_digests(void)
{
    struct digests d = {.cap=8};

    d.names = malloc(d.cap * sizeof(*d.names));
    if (d.names == NULL)
        FAIL_ERRNO("malloc");

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    /* Get digests from active providers in default context. */
    EVP_MD_do_all_provided(NULL, append_digest, &d);
#else
    EVP_MD_do_all(append_digest, &d);
#endif

    qsort(d.names, d.len, sizeof(*d.names), compare);

    for (size_t i = 0; i < d.len; i++)
        puts(d.names[i]);

    free(d.names);
    exit(0);
}
