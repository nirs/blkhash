// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

/* blkhash example program. */

#include <string.h>
#include <openssl/evp.h>
#include <blkhash.h>

#define BLOCK_SIZE (64 * 1024)
#define BUF_SIZE (2 * 1024 * 1024)

int main(int argc, char *argv[])
{
    struct blkhash *h;
    unsigned char *buf;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len, i;

    h = blkhash_new(BLOCK_SIZE, "sha1");

    buf = malloc(BUF_SIZE);

    /* Hash some data. */

    memset(buf, 'A', BUF_SIZE);
    blkhash_update(h, buf, BUF_SIZE);

    /* Hash more data, detecting zero blocks. */

    memset(buf, '\0', BUF_SIZE);
    blkhash_update(h, buf, BUF_SIZE);

    /* Hash 1G of zeros. */

    blkhash_zero(h, 1024 * 1024 * 1024);

    /* Finalize the hash and print a message digest. */

    blkhash_final(h, md, &md_len);

    for (i = 0; i < md_len; i++)
        printf("%02x", md[i]);

    printf("\n");

    blkhash_free(h);
    free(buf);

    return 0;
}
