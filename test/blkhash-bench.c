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

static void print_stats(const char *name, const char *digest, unsigned threads,
                        uint64_t size, int64_t elapsed)
{
    char *hsize, *hrate;
    double seconds;

    seconds = elapsed / 1e6;
    hsize = humansize(size);
    hrate = humansize(size / seconds);

    printf("%s (digest=%s, threads=%d): %s in %.3f seconds (%s/s)\n",
           name, digest, threads, hsize, seconds, hrate);

    free(hsize);
    free(hrate);
}

static void bench(const char *name, const char *digest, unsigned threads,
                  uint64_t size, bool is_zero)
{
    struct blkhash *h;
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    unsigned int len;
    int64_t start, elapsed;
    size_t chunk;
    uint64_t todo;
    int err = 0;

    if (quick)
        size /= 1024;

    todo = size;
    chunk = MIN(size, is_zero ? ZERO_SIZE : READ_SIZE);

    start = gettime();

    h = blkhash_new(digest, BLOCK_SIZE, threads);
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

    print_stats(name, digest, threads, size, elapsed);
}

static void reference(const char *name, const char *digest, uint64_t size)
{
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned char res[EVP_MAX_MD_SIZE];
    unsigned int len;
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

    print_stats(name, digest, 1, size, elapsed);
}

void bench_update_data_sha256()
{
    memset(buf, 0x55, READ_SIZE);

    bench("update-data", "sha256", 1, 0.5 * GiB, false);
    bench("update-data", "sha256", 2, 1.0 * GiB, false);
    bench("update-data", "sha256", 4, 2.0 * GiB, false);
    bench("update-data", "sha256", 8, 2.2 * GiB, false);
    bench("update-data", "sha256", 16, 3.0 * GiB, false);
    bench("update-data", "sha256", 32, 2.9 * GiB, false);
}

void bench_update_data_sha1()
{
    memset(buf, 0x55, READ_SIZE);

    bench("update-data", "sha1", 1, 1.2 * GiB, false);
    bench("update-data", "sha1", 2, 2.2 * GiB, false);
    bench("update-data", "sha1", 4, 4.0 * GiB, false);
    bench("update-data", "sha1", 8, 4.5 * GiB, false);
    bench("update-data", "sha1", 16, 6.0 * GiB, false);
    bench("update-data", "sha1", 32, 5.6 * GiB, false);
}

void bench_update_data_null()
{
    memset(buf, 0x55, READ_SIZE);

    bench("update-data", "null", 1, 10.0 * GiB, false);
    bench("update-data", "null", 2, 16.0 * GiB, false);
    bench("update-data", "null", 4, 23.0 * GiB, false);
    bench("update-data", "null", 8, 23.0 * GiB, false);
    bench("update-data", "null", 16, 22.0 * GiB, false);
    bench("update-data", "null", 32, 22.0 * GiB, false);
}

void bench_update_zero_sha256()
{
    memset(buf, 0, READ_SIZE);

    bench("update-zero", "sha256", 1, 50 * GiB, false);
    bench("update-zero", "sha256", 2, 50 * GiB, false);
    bench("update-zero", "sha256", 4, 50 * GiB, false);
    bench("update-zero", "sha256", 8, 50 * GiB, false);
    bench("update-zero", "sha256", 16, 50 * GiB, false);
    bench("update-zero", "sha256", 31, 50 * GiB, false);
}

void bench_update_zero_sha1()
{
    memset(buf, 0, READ_SIZE);

    bench("update-zero", "sha1", 1, 50 * GiB, false);
    bench("update-zero", "sha1", 2, 50 * GiB, false);
    bench("update-zero", "sha1", 4, 50 * GiB, false);
    bench("update-zero", "sha1", 8, 50 * GiB, false);
    bench("update-zero", "sha1", 16, 50 * GiB, false);
    bench("update-zero", "sha1", 32, 50 * GiB, false);
}

void bench_update_zero_null()
{
    memset(buf, 0, READ_SIZE);

    bench("update-zero", "null", 1, 50 * GiB, false);
    bench("update-zero", "null", 2, 50 * GiB, false);
    bench("update-zero", "null", 4, 50 * GiB, false);
    bench("update-zero", "null", 8, 50 * GiB, false);
    bench("update-zero", "null", 16, 50 * GiB, false);
    bench("update-zero", "null", 32, 50 * GiB, false);
}

void bench_zero_sha256()
{
    bench("zero", "sha256", 1, 860 * GiB, true);
    bench("zero", "sha256", 2, 1.4 * TiB, true);
    bench("zero", "sha256", 4, 2.6 * TiB, true);
    bench("zero", "sha256", 8, 2.9 * TiB, true);
    bench("zero", "sha256", 16, 3.0 * TiB, true);
    bench("zero", "sha256", 32, 3.5 * TiB, true);
}

void bench_zero_sha1()
{
    bench("zero", "sha1", 1, 2.2 * TiB, true);
    bench("zero", "sha1", 2, 3.4 * TiB, true);
    bench("zero", "sha1", 4, 5.2 * TiB, true);
    bench("zero", "sha1", 8, 7.4 * TiB, true);
    bench("zero", "sha1", 16, 9.7 * TiB, true);
    bench("zero", "sha1", 32, 8.4 * TiB, true);
}

void bench_zero_null()
{
    bench("zero", "null", 1, 36 * TiB, true);
    bench("zero", "null", 2, 67 * TiB, true);
    bench("zero", "null", 4, 116 * TiB, true);
    bench("zero", "null", 8, 172 * TiB, true);
    bench("zero", "null", 16, 234 * TiB, true);
    bench("zero", "null", 32, 227 * TiB, true);
}

void bench_sha256()
{
    memset(buf, 0x55, READ_SIZE);
    reference("reference", "sha256", 512 * MiB);
}

void bench_sha1()
{
    memset(buf, 0x55, READ_SIZE);
    reference("reference", "sha1", 1 * GiB);
}

int main(int argc, char *argv[])
{
    /* Minimal test for CI and build machines. */
    quick = (argc > 1 && strcmp(argv[1], "quick") == 0);

    UNITY_BEGIN();

    RUN_TEST(bench_update_data_sha256);
    RUN_TEST(bench_update_data_sha1);
    RUN_TEST(bench_update_data_null);

    RUN_TEST(bench_update_zero_sha256);
    RUN_TEST(bench_update_zero_sha1);
    RUN_TEST(bench_update_zero_null);

    RUN_TEST(bench_zero_sha256);
    RUN_TEST(bench_zero_sha1);
    RUN_TEST(bench_zero_null);

    RUN_TEST(bench_sha256);
    RUN_TEST(bench_sha1);

    return UNITY_END();
}
