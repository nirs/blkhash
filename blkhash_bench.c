// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "blkhash.h"
#include "util.h"

#define READ_SIZE (1 * MiB)
#define ZERO_SIZE MIN(16 * GiB, SIZE_MAX)
#define BLOCK_SIZE (64 * KiB)
#define DIGEST_NAME "sha256"

static unsigned char buf[READ_SIZE];
static bool quick;

void setUp() {}
void tearDown() {}

static void print_stats(const char *name, const char *digest, uint64_t size,
                        int64_t elapsed)
{
    char *hsize, *hrate;
    double seconds;

    seconds = elapsed / 1e6;
    hsize = humansize(size);
    hrate = humansize(size / seconds);

    printf("%s (%s): %s in %.3f seconds (%s/s)\n",
           name, digest, hsize, seconds, hrate);

    free(hsize);
    free(hrate);
}

static void bench(const char *name, const char *digest, uint64_t size, bool
                  is_zero, const char *r, const char *q)
{
    struct blkhash *h;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    unsigned int len;
    char hex[BLKHASH_MAX_MD_SIZE * 2 + 1];
    int64_t start, elapsed;
    size_t chunk;
    uint64_t todo;
    int err = 0;

    if (quick)
        size /= 1024;

    todo = size;
    chunk = MIN(size, is_zero ? ZERO_SIZE : READ_SIZE);

    start = gettime();

    h = blkhash_new(BLOCK_SIZE, digest);
    TEST_ASSERT_NOT_NULL_MESSAGE(h, strerror(errno));

    while (todo >= chunk) {
        if (is_zero)
            err = blkhash_zero(h, chunk);
        else
            err = blkhash_update(h, buf, chunk);
        if (err)
            goto out;
        todo -= chunk;
    }

    if (todo > 0) {
        if (is_zero)
            err = blkhash_zero(h, todo);
        else
            err = blkhash_update(h, buf, todo);
        if (err)
            goto out;
    }

    err = blkhash_final(h, md, &len);

out:
    blkhash_free(h);

    if (err)
        TEST_FAIL_MESSAGE(strerror(err));

    elapsed = gettime() - start;

    format_hex(md, len, hex);

    if (quick)
        TEST_ASSERT_EQUAL_STRING(q, hex);
    else
        TEST_ASSERT_EQUAL_STRING(r, hex);

    print_stats(name, digest, size, elapsed);
}

static void reference(const char *name, const char *digest, uint64_t size,
                      const char *r, const char *q)
{
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned char res[EVP_MAX_MD_SIZE];
    unsigned int len;
    char hex[EVP_MAX_MD_SIZE * 2 + 1];
    int64_t start, elapsed;
    int ok;

    if (quick)
        size = READ_SIZE * 2;

    /* Simplify update loop by requiring aligned size. */
    TEST_ASSERT(size % READ_SIZE == 0);

    start = gettime();

    ctx = EVP_MD_CTX_new();
    TEST_ASSERT_NOT_NULL(ctx);

    md = EVP_get_digestbyname(digest);
    TEST_ASSERT_NOT_NULL(md);

    ok = EVP_DigestInit_ex(ctx, md, NULL);
    TEST_ASSERT(ok);

    for (uint64_t i = 0; i < size; i += READ_SIZE) {
        ok = EVP_DigestUpdate(ctx, buf, READ_SIZE);
        if (!ok)
            goto out;
    }

    ok = EVP_DigestFinal_ex(ctx, res, &len);

out:
    EVP_MD_CTX_free(ctx);

    if (!ok)
        TEST_FAIL_MESSAGE("Error computing checksum");

    elapsed = gettime() - start;

    format_hex(res, len, hex);

    if (quick)
        TEST_ASSERT_EQUAL_STRING(q, hex);
    else
        TEST_ASSERT_EQUAL_STRING(r, hex);

    print_stats(name, digest, size, elapsed);
}

void bench_update_data_sha256()
{
    memset(buf, 0x55, READ_SIZE);
    bench("update-data", "sha256", 2 * GiB, false,
          "b33581f924e62df503920f27b435bf4cead36d8990809c43613182b0c449fb97",
          "96aef7a5e820431384b08c80eaf78a22e8d30d3d4d89906bc6d569963b837f85");
}

void bench_update_data_sha1()
{
    bench("update-data", "sha1", 4 * GiB, false,
          "19f670f7e7866a7b596c7fc0359080ab130a352b",
          "1b4fc3ff10af06488b65af9c18909db166111078");
}

void bench_update_zero_sha256()
{
    memset(buf, 0, READ_SIZE);
    bench("update-zero", "sha256", 50 * GiB, false,
          "b3d7dccca2e9ea73b06ca3b03608e2b47f5f15e5b5baf2b4220b2f92d5be2eac",
          "85eb32dcd84b8e7575c8a3afd6ee1d83d1ded1752b6f61fc0c49f84e97e97eb6");
}

void bench_update_zero_sha1()
{
    memset(buf, 0, READ_SIZE);
    bench("update-zero", "sha1", 50 * GiB, false,
          "5c8cc32fbbee866757e0997d58fd9e3e35c807b0",
          "c15314c3dab3307d6b518ca24c3f15275c03b819");
}

void bench_zero_sha256()
{
    bench("zero", "sha256", 2500 * GiB, true,
          "ee2d710dfa14468ceac5281c57381e50657434da22d9c026244a8b99acc05c1a",
          "e3f123e37abc310ad76b7e775edf4a5707cb8a48214694697267fe2723289893");
}

void bench_zero_sha1()
{
    bench("zero", "sha1", 7500 * GiB, true,
          "cd807d9fa89a6efc66a0ce46408f0689b316d9ee",
          "a9eeed333040ffe0ab284018c5b39f7b5ae390c1");
}

void bench_sha256()
{
    memset(buf, 0x55, READ_SIZE);
    reference("reference", "sha256", 512 * MiB,
              "b51dfe2b75f17ee394307f878b9b161d58f04a9d142411952f97e68b32bc9131",
              "aa5b27f5e2dad9c8a5a6d04320887656484d917c085a4435a919dc2aadc35ec0");
}

void bench_sha1()
{
    memset(buf, 0x55, READ_SIZE);
    reference("reference", "sha1", 1 * GiB,
              "a414a6962c6f87477df2c010ae3627e5f19f0eed",
              "a8f7ffdea89d223c491bc21441c6219be5d1f433");
}

int main(int argc, char *argv[])
{
    /* Minimal test for CI and build machines. */
    quick = (argc > 1 && strcmp(argv[1], "quick") == 0);

    UNITY_BEGIN();

    RUN_TEST(bench_update_data_sha256);
    RUN_TEST(bench_update_data_sha1);
    RUN_TEST(bench_update_zero_sha256);
    RUN_TEST(bench_update_zero_sha1);
    RUN_TEST(bench_zero_sha256);
    RUN_TEST(bench_zero_sha1);

    RUN_TEST(bench_sha256);
    RUN_TEST(bench_sha1);

    return UNITY_END();
}
