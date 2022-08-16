// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "blkhash.h"
#include "util.h"

#define READ_SIZE (256*1024L)
#define BLOCK_SIZE (64*1024L)
#define DIGEST_NAME "sha256"

#define MiB (1L<<20)
#define GiB (1L<<30)
#define TiB (1L<<40)

static unsigned char buf[READ_SIZE];
static bool quick;

void bench(const char *name, const char *digest, uint64_t size, bool is_zero)
{
    struct blkhash *h;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len;
    int64_t start, elapsed;
    double seconds;
    double rate;

    if (quick)
        size /= 100;

    start = gettime();

    h = blkhash_new(BLOCK_SIZE, digest);
    assert(h);

    for (uint64_t i = 0; i < size / READ_SIZE; i++) {
        if (is_zero)
            blkhash_zero(h, READ_SIZE);
        else
            blkhash_update(h, buf, READ_SIZE);
    }

    blkhash_final(h, md, &len);
    blkhash_free(h);

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    rate = size / seconds / GiB;

    printf("%s (%s): %.2f GiB in %.2f seconds (%.2f GiB/s)\n",
           name, digest, (double)size / GiB, seconds, rate);
}

void bench_update_data()
{
    memset(buf, 0x55, READ_SIZE);
    bench("update data", "sha256", 2 * GiB, false);
    bench("update data", "sha1", 4 * GiB, false);
}

void bench_update_zero()
{
    memset(buf, 0, READ_SIZE);
    bench("update zero", "sha256", 50 * GiB, false);
    bench("update zero", "sha1", 50 * GiB, false);
}

void bench_zero()
{
    bench("zero", "sha256", 2500 * GiB, true);
    bench("zero", "sha1", 7500 * GiB, true);
}

int main(int argc, char *argv[])
{
    /* Minimal test for CI and build machines. */
    quick = (argc > 1 && strcmp(argv[1], "quick") == 0);

    bench_update_data();
    bench_update_zero();
    bench_zero();
}
