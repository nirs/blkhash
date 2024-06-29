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

static const unsigned threads = 64;
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

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "2e3a5f57d5e726ee5de62db1a85ff816f4fa6ea4627807d9a51bdc4c883e165e");
    }
}

void test_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "8ab067ab7939a3d4f40cbb118956c07c06aeae2d075d6e0a2ebbb94bd42832af");
    }
}

void test_block_zero()
{
    struct extent extents[] = {
        {'-', block_size},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "8ab067ab7939a3d4f40cbb118956c07c06aeae2d075d6e0a2ebbb94bd42832af");
    }
}

void test_partial_block_data()
{
    struct extent extents[] = {
        {'A', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "9467334d6d7668a2f06898185b5cddc78cdfbd962f50c6bf250101f03a0e54dc");
    }
}

void test_partial_block_data_zero()
{
    struct extent extents[] = {
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "842d185f0ff348b0d689e184bddc47f69a19032a6934c89b879479f1e86f9d56");
    }
}

void test_partial_block_zero()
{
    struct extent extents[] = {
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "842d185f0ff348b0d689e184bddc47f69a19032a6934c89b879479f1e86f9d56");
    }
}

void test_sparse()
{
    struct extent extents[] = {
        {'-', block_size * 8},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "722ac11a3a81034edf196aa3ee68d1a2e345cd0838b0c78c2ce2fb015d4d817d");
    }
}

void test_sparse_large()
{
    struct extent extents[] = {
        {'-', 1024 * 1024 * 1024},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "c6d6562c3074a2caa0ceea3e1f460f3af678c792ed05e64157eb51e68ae5260d");
    }
}

void test_sparse_unaligned()
{
    struct extent extents[] = {
        {'-', block_size * 8},
        {'-', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "e623a5702b1feb08fbb3d18e35bbb7ec10146b8cd1bcff811200a3948c78c55c");
    }
}

void test_zero()
{
    struct extent extents[] = {
        {'\0', block_size * 8},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "722ac11a3a81034edf196aa3ee68d1a2e345cd0838b0c78c2ce2fb015d4d817d");
    }
}

void test_zero_unaligned()
{
    struct extent extents[] = {
        {'\0', block_size * 8},
        {'\0', block_size / 2},
    };
    char hexdigest[hexdigest_len];

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "e623a5702b1feb08fbb3d18e35bbb7ec10146b8cd1bcff811200a3948c78c55c");
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

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "4ab1586084326f5beb911621282e9557cd26856105eff7f78cace5986af4738b");
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

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "2cad558b3a2a8d87435e80fe7bae69fa664d92eaa1b9411f573b9fbb412a4d43");
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

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "4b2b5acd96f8105f4fb26abaf35c7c5756338ef9ccce19ffe7714c9ad94b4398");
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

    for (unsigned i = 1; i <= threads; i*=2) {
        checksum(extents, ARRAY_SIZE(extents), digest_name, block_size, i,
                 hexdigest);
        TEST_ASSERT_EQUAL_STRING(
            hexdigest,
            "2510ac29c7103f2e68c672d728cbd43db4ed3dc809a24cde96e62dc5862a0f57");
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
