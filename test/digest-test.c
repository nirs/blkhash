// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <string.h>

#include "digest.h"
#include "unity.h"
#include "util.h"

struct test_vector {
    const char *name;
    const char *message;
    int count;
    const char *output;
};

void setUp() {}
void tearDown() {}

static void check(const char *algorithm, struct test_vector *tv)
{
    struct digest *d = NULL;
    unsigned int message_len = strlen(tv->message);
    unsigned char md[DIGEST_MAX_MD_SIZE];
    char output[DIGEST_MAX_MD_SIZE * 2 + 1];
    unsigned int md_len = 0;
    int err;

    err = digest_create(algorithm, &d);
    if (err)
        goto out;

    err = digest_init(d);
    if (err)
        goto out;

    for (int i = 0; i < tv->count; i++) {
        err = digest_update(d, tv->message, message_len);
        if (err)
            goto out;
    }

    err = digest_final(d, md, &md_len);

out:
    digest_destroy(d);

    TEST_ASSERT_EQUAL_INT(0, err);

    format_hex(md, md_len, output);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(tv->output, output, tv->name);
}

void test_list()
{
    const char *names;
    size_t len;
    int err;

    err = digest_list(&names, &len);

    TEST_ASSERT_EQUAL_INT(0, err);

    /* TODO: deal with unpredictable return value from openssl. */
    TEST_ASSERT_EQUAL_INT(1, len);
    TEST_ASSERT_EQUAL_STRING("null", &names[0]);
}

void test_unknown()
{
    struct digest *d;
    int err;

    err = digest_create("unknown-digest", &d);
    TEST_ASSERT_EQUAL_INT(-EINVAL, err);
}

void test_null()
{
    struct test_vector tests[] = {
        {
            .name="empty string",
            .message="",
            .count=1,
            .output="",
        },
        {
            .name="abc",
            .message="abc",
            .count=1,
            .output="",
        },
        {
            .name="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .message="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .count=1,
            .output="",
        },
        {
            .name="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .message="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .count=1,
            .output="",
        },
        {
            .name="'a' repeated 1,000,000 times",
            .message="a",
            .count=1000000,
            .output="",
        },
        {
            .name="'abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno' repeated 16,777,216 times",
            .message="a",
            .count=16777216,
            .output="",
        },
    };

    for (unsigned i = 0; i < ARRAY_SIZE(tests); i++) {
        check("null", &tests[i]);
    }
}

void test_null_no_size()
{
    struct digest *d;
    int err;

    err = digest_create("null", &d);
    if (err)
        goto out;

    err = digest_init(d);
    if (err)
        goto out;

    /* Buffed not modified */
    err = digest_final(d, NULL, NULL);

out:
    digest_destroy(d);

    TEST_ASSERT_EQUAL_INT(0, err);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_list);
    RUN_TEST(test_unknown);
    RUN_TEST(test_null);
    RUN_TEST(test_null_no_size);

    return UNITY_END();
}
