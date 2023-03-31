// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "unity.h"
#include "blkhash.h"
#include "blkhash-internal.h"
#include "util.h"

static const size_t block_size = 64 * 1024;
static const char * digest_name = "sha256";
static const unsigned int digest_len = 32;
static const unsigned int hexdigest_len = digest_len * 2 + 1; /* NULL */
static const unsigned streams = BLKHASH_STREAMS;

void setUp() {}
void tearDown() {}

struct extent {
    char byte;
    unsigned int len;
};

void checksum(struct extent *extents, unsigned int len,
              const char *digest_name, size_t block_size, unsigned threads,
              char *hexdigest)
{
    unsigned char md[digest_len];
    unsigned int md_len = digest_len;
    struct blkhash *h;
    struct blkhash_opts *opts;
    int err = 0;

    opts = blkhash_opts_new(digest_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(opts, strerror(errno));
    err = blkhash_opts_set_block_size(opts, block_size);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, err, strerror(err));
    err = blkhash_opts_set_streams(opts, streams);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, err, strerror(err));
    err = blkhash_opts_set_threads(opts, threads);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, err, strerror(err));

    h = blkhash_new_opts(opts);
    blkhash_opts_free(opts);
    TEST_ASSERT_NOT_NULL_MESSAGE(h, strerror(errno));

    for (unsigned i = 0; i < len; i++) {
        struct extent *e = &extents[i];

        if (e->byte == '-') {
            err = blkhash_zero(h, e->len);
            if (err)
                goto out;
        } else {
            unsigned char *buf;

            buf = malloc(e->len);
            if (buf == NULL) {
                err = errno;
                goto out;
            }

            memset(buf, e->byte, e->len);

            err = blkhash_update(h, buf, e->len);
            free(buf);
            if (err)
                goto out;
        }
    }

    err = blkhash_final(h, md, &md_len);
    if (err)
        goto out;

    format_hex(md, md_len, hexdigest);

out:
    blkhash_free(h);
    if (err)
        TEST_FAIL_MESSAGE(strerror(err));
}

void test_block_data()
{
    struct extent extents[] = {
        {'A', block_size},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "3fe9a19c59cc0320c1bb605e3cbf3ecd35a295a1f7a2b4e5ebc1efdd1f5ebb8c",
            hexdigest);
    }
}

void test_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "62e22dfa9a68d2747441335d07929c1577a0a836a90cb9bddc016f1728ae0ae6",
            hexdigest);
    }
}

void test_block_zero()
{
    struct extent extents[] = {
        {'-', block_size},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "62e22dfa9a68d2747441335d07929c1577a0a836a90cb9bddc016f1728ae0ae6",
            hexdigest);
    }
}

void test_partial_block_data()
{
    struct extent extents[] = {
        {'A', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "cd8d1ced5e8cb96831fc637e7c69a4ce940b04be401b30be7fabba4451c6e4c0",
            hexdigest);
    }
}

void test_partial_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "eb6df5009bff0bd2f11a42fdfee2f24ab88b8e2c4d8cd3fac686ecbb06a91c60",
            hexdigest);
    }
}

void test_partial_block_zero()
{
    struct extent extents[] = {
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "eb6df5009bff0bd2f11a42fdfee2f24ab88b8e2c4d8cd3fac686ecbb06a91c60",
            hexdigest);
    }
}

void test_sparse()
{
    struct extent extents[] = {
        {'-', block_size * 8},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "ac5b87337b903179e34e03ddddaa2132a5ff0733922a2c697b322416529ab50b",
            hexdigest);
    }
}

void test_sparse_large()
{
    struct extent extents[] = {
        {'-', 1024 * 1024 * 1024},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "cd8516de3da285cff382e4bd7528d148d8650691b5b78d77e0ebf8a6609c7aa0",
            hexdigest);
    }
}

void test_sparse_unaligned()
{
    struct extent extents[] = {
        {'-', block_size * 8},
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "904281f5a6b3a2cddde0ef22fcd904a7210354b637365f6f6b21bb0b2ae230cb",
            hexdigest);
    }
}

void test_zero()
{
    struct extent extents[] = {
        {'\0', block_size * 8},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "ac5b87337b903179e34e03ddddaa2132a5ff0733922a2c697b322416529ab50b",
            hexdigest);
    }
}

void test_zero_unaligned()
{
    struct extent extents[] = {
        {'\0', block_size * 8},
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "904281f5a6b3a2cddde0ef22fcd904a7210354b637365f6f6b21bb0b2ae230cb",
            hexdigest);
    }
}

void test_full()
{
    struct extent extents[] = {
        {'A', block_size / 2},
        {'B', block_size / 2},
        {'C', block_size / 2},
        {'D', block_size / 2},
        {'E', block_size / 2},
        {'F', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "51dce4f28453cb4087dd506441da279916b60f6875e51ca4df5d2184de69509b",
            hexdigest);
    }
}

void test_full_unaligned()
{
    struct extent extents[] = {
        {'A', block_size / 2},
        {'B', block_size / 2},
        {'C', block_size / 2},
        {'D', block_size / 2},
        {'E', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "35b8503c0575e3b81a7d5f7f03bb9e10a2baa0b86d871e26ae9d458ff1b5e656",
            hexdigest);
    }
}

void test_mix()
{
    struct extent extents[] = {
        /* Add pending data... */
        {'A', block_size / 2},
        /* Add zeros converting zeros to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeros... */
        {'-', block_size / 2},
        /* Add data, converting pending zeros to data and consume pending. */
        {'\0', block_size / 2},
        /* Add pending data... */
        {'E', block_size / 2},
        /* Add zeros, converting zeros to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeros... */
        {'-', block_size / 2},
        /* Add data, converting pending zeros to data and consume pending. */
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "6a2f51e242d77a19e966b3ecad98c63dbe0bb2ff984c4ba70fb52c6ef9956897",
            hexdigest);
    }
}

void test_mix_unaligned()
{
    struct extent extents[] = {
        /* Add pending data... */
        {'A', block_size / 2},
        /* Add zeros converting zeros to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeros... */
        {'-', block_size / 2},
        /* Add data, converting pending zeros to data and consume pending. */
        {'\0', block_size / 2},
        /* Add pending data... */
        {'E', block_size / 2},
        /* Add zeros, converting zeros to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeros... */
        {'-', block_size / 2},
        /* Consume pending zeros. */
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= streams; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            "3dc7e5448806207aa096baccdec8d2c0a27986ebf31a4a7e96e9ec9ca334eb84",
            hexdigest);
    }
}

void test_abort_quickly()
{
    struct blkhash *h;
    int err;

    h = blkhash_new();
    TEST_ASSERT_NOT_NULL_MESSAGE(h, strerror(errno));

    for (int i = 0; i < 10; i++) {
        err = blkhash_zero(h, 3 * GiB);
        if (err)
            break;
    }

    blkhash_free(h);

    /* TODO: check that workers were stopped without doing any work. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, err, strerror(err));
}

static void check_false_sharing(const char *name, size_t type_size)
{
    if (type_size % CACHE_LINE_SIZE != 0) {
        size_t padding = CACHE_LINE_SIZE - (type_size % CACHE_LINE_SIZE);

        printf("WARNING: %s is not aligned to cache line size\n", name);
        printf("  type size: %ld\n", type_size);
        printf("  cache line size: %d\n", CACHE_LINE_SIZE);
        printf("  needs padding: %ld\n", padding);
    }
}

void test_false_sharing()
{
    check_false_sharing("struct stream", sizeof(struct stream));
    check_false_sharing("struct config", sizeof(struct config));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_block_data);
    RUN_TEST(test_block_data_zero);
    RUN_TEST(test_block_zero);

    RUN_TEST(test_partial_block_data);
    RUN_TEST(test_partial_block_data_zero);
    RUN_TEST(test_partial_block_zero);

    RUN_TEST(test_sparse);
    RUN_TEST(test_sparse_unaligned);
    RUN_TEST(test_sparse_large);

    RUN_TEST(test_zero);
    RUN_TEST(test_zero_unaligned);

    RUN_TEST(test_full);
    RUN_TEST(test_full_unaligned);

    RUN_TEST(test_mix);
    RUN_TEST(test_mix_unaligned);

    RUN_TEST(test_abort_quickly);

    RUN_TEST(test_false_sharing);

    return UNITY_END();
}
