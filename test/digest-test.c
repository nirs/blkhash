// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <string.h>

#include "blkhash.h"
#include "blkhash-config.h"
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
    unsigned char md[BLKHASH_MAX_MD_SIZE];
    char output[BLKHASH_MAX_MD_SIZE * 2 + 1];
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

void test_blake3()
{
    struct test_vector tests[] = {
        {
            .name="empty string",
            .message="",
            .count=1,
            .output="af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262",
        },
        {
            .name="abc",
            .message="abc",
            .count=1,
            .output="6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85",
        },
        {
            .name="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .message="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            .count=1,
            .output="c19012cc2aaf0dc3d8e5c45a1b79114d2df42abb2a410bf54be09e891af06ff8",
        },
        {
            .name="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .message="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            .count=1,
            .output="553e1aa2a477cb3166e6ab38c12d59f6c5017f0885aaf079f217da00cfca363f",
        },
        {
            .name="'a' repeated 1,000,000 times",
            .message="a",
            .count=1000000,
            .output="616f575a1b58d4c9797d4217b9730ae5e6eb319d76edef6549b46f4efe31ff8b",
        },
        {
            .name="'abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno' repeated 16,777,216 times",
            .message="a",
            .count=16777216,
            .output="38ed7ad5a524e76d65dc389f1262b999fdc796c7e5d70132c4f9a049dcf3f127",
        },
    };

    for (unsigned i = 0; i < ARRAY_SIZE(tests); i++) {
        check("blake3", &tests[i]);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_unknown);
    RUN_TEST(test_null);
    RUN_TEST(test_null_no_size);
    RUN_TEST(test_sha256);
    RUN_TEST(test_blake2b512);

#ifdef HAVE_BLAKE3
    RUN_TEST(test_blake3);
#endif

    return UNITY_END();
}
