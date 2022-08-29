// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "blkhash.h"
#include "util.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static const size_t block_size = 64 * 1024;
static const char * digest_name = "sha256";
static const unsigned int digest_len = 32;
static const unsigned int hexdigest_len = digest_len * 2 + 1; /* NULL */

void setUp() {}
void tearDown() {}

struct extent {
    char byte;
    unsigned int len;
};

void checksum(struct extent *extents, unsigned int len,
              size_t block_size, const char *digest_name, char *hexdigest)
{
    unsigned char md[digest_len];
    unsigned int md_len = digest_len;
    struct blkhash *h = blkhash_new(block_size, digest_name);
    assert(h != NULL);

    for (int i = 0; i < len; i++) {
        struct extent *e = &extents[i];

        if (e->byte == '-') {
            blkhash_zero(h, e->len);
        } else {
            unsigned char *buf = malloc(e->len);
            assert(buf != NULL);
            memset(buf, e->byte, e->len);

            blkhash_update(h, buf, e->len);

            free(buf);
        }
    }

    blkhash_final(h, md, &md_len);
    blkhash_free(h);

    format_hex(md, md_len, hexdigest);
}

void test_block_data()
{
    struct extent extents[] = {
        {'A', block_size},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "b1a37d57301efe26db0ae98c681fb33bc7718e2d7eaa6d14bef667fdb0ce4153",
        hexdigest);
}

void test_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "e3464a69bf8aa32beb68088f07a797b61edba57b87bcccb19e4b093ded09d2c3",
        hexdigest);
}

void test_block_zero()
{
    struct extent extents[] = {
        {'-', block_size},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "e3464a69bf8aa32beb68088f07a797b61edba57b87bcccb19e4b093ded09d2c3",
        hexdigest);
}

void test_partial_block_data()
{
    struct extent extents[] = {
        {'A', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "9882fe93f0340c4414833acadae9c0dcf1c988e2cf1da67902e6863f069c2617",
        hexdigest);
}

void test_partial_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "982e8f30451ead173a4da1df76e3b8849a3d0a5126f03e09b54e7c107c429b01",
        hexdigest);
}

void test_partial_block_zero()
{
    struct extent extents[] = {
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "982e8f30451ead173a4da1df76e3b8849a3d0a5126f03e09b54e7c107c429b01",
        hexdigest);
}

void test_sparse()
{
    struct extent extents[] = {
        {'-', block_size * 8},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "823d6ac7d26b7768abfbd2051a6bb167937043e884bac39ea8da31bae7bf5ace",
        hexdigest);
}

void test_sparse_large()
{
    struct extent extents[] = {
        {'-', 1024 * 1024 * 1024},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "9b3d2f329b8e1a3a10ac623efa163c12e953dbb5192825b4772dcf0f8905e1b1",
        hexdigest);
}

void test_sparse_unaligned()
{
    struct extent extents[] = {
        {'-', block_size * 8},
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "d28c351b1e0d8293aace1032ccee33579fbaf3075e0d5e868226bf9d898cc476",
        hexdigest);
}

void test_zero()
{
    struct extent extents[] = {
        {'\0', block_size * 8},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "823d6ac7d26b7768abfbd2051a6bb167937043e884bac39ea8da31bae7bf5ace",
        hexdigest);
}

void test_zero_unaligned()
{
    struct extent extents[] = {
        {'\0', block_size * 8},
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "d28c351b1e0d8293aace1032ccee33579fbaf3075e0d5e868226bf9d898cc476",
        hexdigest);
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
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "658d47f67ee57ce66c71fccc5ebf7768f5720c9c37139409874d8afe354a9571",
        hexdigest);
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
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "d08e319cba087440b6f42120df4a8830b2475463edf2967cc61f3cd6ccaa84c6",
        hexdigest);
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
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "fe6ed2020798c76a9a28e98c4a575f12a29710f41ec88f1055fa5b407361085a",
        hexdigest);
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
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "8b4034f448346b3feeb89b08d15a07feeba8de3baaeda47ecc15d3dd16d8c4ca",
        hexdigest);
}

void test_abort_quickly()
{
    struct blkhash *h = blkhash_new(block_size, digest_name);
    assert(h != NULL);

    for (int i = 0; i < 10; i++)
        blkhash_zero(h, 3 * GiB);

    blkhash_free(h);

    /* TODO: check that workers were stopped without doing any work. */
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

    return UNITY_END();
}
