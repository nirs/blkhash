// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blkhash-config.h"
#include "blkhash-internal.h"
#include "blkhash.h"
#include "submission.h"
#include "unity.h"
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

static const char *lookup(const char **array, size_t len, const char *item)
{
    for (unsigned i = 0; i < len; i++) {
        if (strcmp(array[i], item) == 0)
            return array[i];
    }

    return NULL;
}

void test_digests()
{
    const char *expected[] = {
        "null",
        "blake2b512",
        "blake2s256",
#ifdef HAVE_BLAKE3
        "blake3",
#endif
        "sha224",
        "sha256",
        "sha384",
        "sha512",
        "sha512-224",
        "sha512-256",
        "sha3-224",
        "sha3-256",
        "sha3-384",
        "sha3-512",
    };
    const char **names;
    size_t len;

    len = blkhash_digests(NULL, 0);

    names = calloc(len, sizeof(*names));
    TEST_ASSERT_NOT_NULL(names);

    blkhash_digests(names, len);

    for (unsigned i = 0; i < ARRAY_SIZE(expected); i++) {
        const char *found = lookup(names, len, expected[i]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expected[i], found, "digest not found");
    }

    free(names);
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
            hexdigest,
            "70eac04b8330211be9e1737b711e241fb3233a7d372ae9819f1aab6cf525a74b");
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
            hexdigest,
            "63aa20dde75b6c4756c8e10760f60ce817247f6fa7d1077977e606cc47ae6e15");
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
            hexdigest,
            "63aa20dde75b6c4756c8e10760f60ce817247f6fa7d1077977e606cc47ae6e15");
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
            hexdigest,
            "5f83249849f938a21ddb2a3c2edd090bb152e1b7a4d667032c5e9ca42ef55385");
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
            hexdigest,
            "1a305cd50790418c9fc0904a5677bedc0aa7cdcbae43a080dd71388b6feca68d");
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
            hexdigest,
            "1a305cd50790418c9fc0904a5677bedc0aa7cdcbae43a080dd71388b6feca68d");
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
            hexdigest,
            "20d599e6ed9c6ff44a78fef9af8d3c696a556cf535fcb0bba3ef6d519d7238e7");
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
            hexdigest,
            "114987e2287276ffdd3dab2426a630a84d060cf01cd7d8e9d1bfa7ddb78ba5ad");
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
            hexdigest,
            "310bf52aebda5c7c6b4a7725ee4fe528c369273bab1ed1f6a76f53f187bc0aec");
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
            hexdigest,
            "20d599e6ed9c6ff44a78fef9af8d3c696a556cf535fcb0bba3ef6d519d7238e7");
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
            hexdigest,
            "310bf52aebda5c7c6b4a7725ee4fe528c369273bab1ed1f6a76f53f187bc0aec");
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
            hexdigest,
            "cafbb8ce5060e31d858ed85b09fe38ae71d79ad5a210d56957a5a384c5a6f34d");
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
            hexdigest,
            "5f7db317939052a697b1f56e15557e60fdd1b31e8b1363bda1d6913e862759cc");
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
            hexdigest,
            "a4b396c7f9ab5a902777418fb1ff3a485de459c6478a40a710ffa8eac3358517");
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
            hexdigest,
            "1aaa8cefa00b5ba1f33a39e239e68d66b28eae90db66065a5137166ebaecdcca");
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
    check_false_sharing("struct config", sizeof(struct config));
    check_false_sharing("struct submission", sizeof(struct submission));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_digests);

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
