// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

/* blkhash example program. */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <blkhash.h>

#define BLOCK_SIZE (64 * 1024)
#define THREADS 4
#define BUF_SIZE (2 * 1024 * 1024)

int main()
{
    struct blkhash *h = NULL;
    unsigned char *buf = NULL;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    unsigned int md_len, i;
    int err;

    h = blkhash_new("sha1", BLOCK_SIZE, THREADS);
    if (h == NULL) {
        err = errno;
        goto out;
    }

    buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        err = errno;
        goto out;
    }

    /* Hash some data. */

    memset(buf, 'A', BUF_SIZE);
    err = blkhash_update(h, buf, BUF_SIZE);
    if (err)
        goto out;

    /* Hash more data, detecting zero blocks. */

    memset(buf, '\0', BUF_SIZE);
    err = blkhash_update(h, buf, BUF_SIZE);
    if (err)
        goto out;

    /* Hash 1G of zeros. */

    err = blkhash_zero(h, 1024 * 1024 * 1024);
    if (err)
        goto out;

    /* Finalize the hash and print a message digest. */

    err = blkhash_final(h, md, &md_len);
    if (err)
        goto out;

    for (i = 0; i < md_len; i++)
        printf("%02x", md[i]);

    printf("\n");

out:
    blkhash_free(h);
    free(buf);

    if (err) {
        fprintf(stderr, "Example failed: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    return 0;
}
