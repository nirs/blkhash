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

static void bench(const char *name, const char *digest, uint64_t size,
                  bool is_zero)
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

    print_stats(name, digest, size, elapsed);
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

    print_stats(name, digest, size, elapsed);
}

void bench_update_data_sha256()
{
    memset(buf, 0x55, READ_SIZE);
    bench("update-data", "sha256", 2 * GiB, false);
}

void bench_update_data_sha1()
{
    bench("update-data", "sha1", 4 * GiB, false);
}

void bench_update_data_null()
{
    memset(buf, 0x55, READ_SIZE);
    bench("update-data", "null", 23 * GiB, false);
}

void bench_update_zero_sha256()
{
    memset(buf, 0, READ_SIZE);
    bench("update-zero", "sha256", 50 * GiB, false);
}

void bench_update_zero_sha1()
{
    memset(buf, 0, READ_SIZE);
    bench("update-zero", "sha1", 50 * GiB, false);
}

void bench_update_zero_null()
{
    memset(buf, 0, READ_SIZE);
    bench("update-zero", "null", 50 * GiB, false);
}

void bench_zero_sha256()
{
    bench("zero", "sha256", 2500 * GiB, true);
}

void bench_zero_sha1()
{
    bench("zero", "sha1", 7500 * GiB, true);
}

void bench_zero_null()
{
    bench("zero", "null", 110 * TiB, true);
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
