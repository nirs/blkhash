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
static const char * digest_name = "sha1";
static const unsigned int digest_len = 20;
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
    TEST_ASSERT_EQUAL_STRING("7f18f4ceb77fd099a58fd2b5e3e6fbe959e07d77", hexdigest);
}

void test_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("cb29e710160d736b446eba4fc235ce3dc0ea5542", hexdigest);
}

void test_block_zero()
{
    struct extent extents[] = {
        {'-', block_size},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("cb29e710160d736b446eba4fc235ce3dc0ea5542", hexdigest);
}

void test_partial_block_data()
{
    struct extent extents[] = {
        {'A', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("d00e4e24b73cdbfe1e85d70926f90386d5a1ca99", hexdigest);
}

void test_partial_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("f3181c76c4a5094b6fd65ec1d186cb1ec35bcfa6", hexdigest);
}

void test_partial_block_zero()
{
    struct extent extents[] = {
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("f3181c76c4a5094b6fd65ec1d186cb1ec35bcfa6", hexdigest);
}

void test_sparse()
{
    struct extent extents[] = {
        {'-', block_size * 3},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("abf5dc2d9aba57fd01b38403c16d3d0dbf9bd341", hexdigest);
}

void test_sparse_unaligned()
{
    struct extent extents[] = {
        {'-', block_size * 2},
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("bb54c3bea2b1e6aedbeed5270e910fc96c26bec6", hexdigest);
}

void test_sparse_large()
{
    struct extent extents[] = {
        {'-', 1024 * 1024 * 1024},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("fe623c97a1b52b9263043bf11e9ec686dab34da9", hexdigest);
}

void test_zero()
{
    struct extent extents[] = {
        {'\0', block_size * 3},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("abf5dc2d9aba57fd01b38403c16d3d0dbf9bd341", hexdigest);
}

void test_zero_unaligned()
{
    struct extent extents[] = {
        {'\0', block_size * 2},
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("bb54c3bea2b1e6aedbeed5270e910fc96c26bec6", hexdigest);
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
    TEST_ASSERT_EQUAL_STRING("4c66cc478df1ad296bcd6e4f28620f5556b13ca3", hexdigest);
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
    TEST_ASSERT_EQUAL_STRING("cb2c09df8d1e976768307c63cc764b2fb52679f8", hexdigest);
}

void test_mix()
{
    struct extent extents[] = {
        /* Add pending data... */
        {'A', block_size / 2},
        /* Add zeroes converting zeroes to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeroes... */
        {'-', block_size / 2},
        /* Add data, converting pending zeroes to data and consume pending. */
        {'\0', block_size / 2},
        /* Add pending data... */
        {'E', block_size / 2},
        /* Add zeroes, converting zeroes to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeroes... */
        {'-', block_size / 2},
        /* Add data, converting pending zeroes to data and consume pending. */
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("7a64eb9e5b568a4c912f9749aadd305fb463e614", hexdigest);
}

void test_mix_unaligned()
{
    struct extent extents[] = {
        /* Add pending data... */
        {'A', block_size / 2},
        /* Add zeroes converting zeroes to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeroes... */
        {'-', block_size / 2},
        /* Add data, converting pending zeroes to data and consume pending. */
        {'\0', block_size / 2},
        /* Add pending data... */
        {'E', block_size / 2},
        /* Add zeroes, converting zeroes to data and consume pending. */
        {'-', block_size / 2},
        /* Add pending zeroes... */
        {'-', block_size / 2},
        /* Consume pending zeroes. */
    };
    char hexdigest[hexdigest_len];
    checksum(extents, ARRAY_SIZE(extents), block_size, digest_name, hexdigest);
    TEST_ASSERT_EQUAL_STRING("dbaf0d2128c147d032842474c04918cc57f8da68", hexdigest);
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

    return UNITY_END();
}
