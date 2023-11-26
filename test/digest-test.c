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

static const char *lookup(const char **array, size_t len, const char *item)
{
    for (unsigned i = 0; i < len; i++) {
        if (strcmp(array[i], item) == 0)
            return array[i];
    }

    return NULL;
}

void test_list()
{
    const char *expected[] = {
        "null",
        "blake2b512",
        "blake2s256",
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

    len = digest_list(NULL, 0);

    names = calloc(len, sizeof(*names));
    TEST_ASSERT_NOT_NULL(names);

    digest_list(names, len);

    for (unsigned i = 0; i < ARRAY_SIZE(expected); i++) {
        const char *found = lookup(names, len, expected[i]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expected[i], found, "digest not found");
    }

    free(names);
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

void test_sha256()
{
    struct test_vector tests[] = {
        {
            .name="empty string",
            .message="",
            .count=1,
            .output="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        },
        {
            .name="abc",
            .message="abc",
            .count=1,
            .output="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        },
        {
            .name="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .message="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .count=1,
            .output="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        },
        {
            .name="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .message="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .count=1,
            .output="cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
        },
        {
            .name="'a' repeated 1,000,000 times",
            .message="a",
            .count=1000000,
            .output="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        },
        {
            .name="'abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno' repeated 16,777,216 times",
            .message="a",
            .count=16777216,
            .output="5b6ff2e19d0da0fe323061018fc381393492884e74af8296c81ab9cb2694783a",
        },
    };

    for (unsigned i = 0; i < ARRAY_SIZE(tests); i++) {
        check("sha256", &tests[i]);
    }
}

void test_blake2b512()
{
    struct test_vector tests[] = {
        {
            .name="empty string",
            .message="",
            .count=1,
            .output="786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce",
        },
        {
            .name="abc",
            .message="abc",
            .count=1,
            .output="ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923",
        },
        {
            .name="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .message="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .count=1,
            .output="7285ff3e8bd768d69be62b3bf18765a325917fa9744ac2f582a20850bc2b1141ed1b3e4528595acc90772bdf2d37dc8a47130b44f33a02e8730e5ad8e166e888",
        },
        {
            .name="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .message="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .count=1,
            .output="ce741ac5930fe346811175c5227bb7bfcd47f42612fae46c0809514f9e0e3a11ee1773287147cdeaeedff50709aa716341fe65240f4ad6777d6bfaf9726e5e52",
        },
        {
            .name="'a' repeated 1,000,000 times",
            .message="a",
            .count=1000000,
            .output="98fb3efb7206fd19ebf69b6f312cf7b64e3b94dbe1a17107913975a793f177e1d077609d7fba363cbba00d05f7aa4e4fa8715d6428104c0a75643b0ff3fd3eaf",
        },
        {
            .name="'abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno' repeated 16,777,216 times",
            .message="a",
            .count=16777216,
            .output="096ce550e899a2b88cad350fa43175e24592a547cdace5e6330167bff92a2db427887d4221fae00ca681a323ebd18e6d67678c1f3ab6c2a00e09fbec601afdb9",
        },
    };

    for (unsigned i = 0; i < ARRAY_SIZE(tests); i++) {
        check("blake2b512", &tests[i]);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_list);
    RUN_TEST(test_unknown);
    RUN_TEST(test_null);
    RUN_TEST(test_null_no_size);
    RUN_TEST(test_sha256);
    RUN_TEST(test_blake2b512);

    return UNITY_END();
}
