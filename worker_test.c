// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "blkhash_internal.h"
#include "unity.h"
#include "util.h"

#define DIGEST_NAME "sha256"
#define BLOCK_SIZE (64 * 1024)
#define BLOCKS 8

/* Test data */
static unsigned char data[BLOCKS][BLOCK_SIZE];

static unsigned char md_value[EVP_MAX_MD_SIZE];
static unsigned int md_len;
static char hexdigest[EVP_MAX_MD_SIZE * 2 + 1];
static struct config *cfg;
static EVP_MD_CTX *ctx;

void setUp()
{
    for (int i = 0; i < BLOCKS; i++)
        memset(data[i], 'A' + i, BLOCK_SIZE);

    memset(md_value, 0, sizeof(md_value));
    memset(hexdigest, 0, sizeof(hexdigest));

    ctx = EVP_MD_CTX_new();
    assert(ctx);
}

void tearDown()
{
    EVP_MD_CTX_free(ctx);
    ctx = NULL;

    config_free(cfg);
    cfg = NULL;
}

/*
 * One worker - processing all blocks.
 */

void test_1_aligned_full()
{
    cfg = config_new(DIGEST_NAME, BLOCK_SIZE, 1);
    assert(cfg);

    struct worker w;
    worker_init(&w, 0, cfg);

    for (int i = 0; i < BLOCKS; i++) {
        struct block *b = block_new(i, cfg->block_size, data[i]);
        assert(worker_update(&w, b) == 0);
    }

    worker_final(&w, BLOCKS * cfg->block_size);
    worker_digest(&w, md_value, &md_len);

    if (!EVP_Digest(md_value, md_len, md_value, &md_len, cfg->md, NULL))
        TEST_FAIL_MESSAGE("EVP_Digest failed");

    worker_destroy(&w);

    format_hex(md_value, md_len, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "3ec07d037d5223e4f8fc139c6bb7505eb20a927a7bba96c6854544a5a2d05f67",
        hexdigest);
}

void test_1_aligned_sparse()
{
    cfg = config_new(DIGEST_NAME, BLOCK_SIZE, 1);
    assert(cfg);

    struct worker w;
    worker_init(&w, 0, cfg);

    /* Skipping all blocks... */

    worker_final(&w, BLOCKS * cfg->block_size);
    worker_digest(&w, md_value, &md_len);
    worker_destroy(&w);

    if (!EVP_Digest(md_value, md_len, md_value, &md_len, cfg->md, NULL))
        TEST_FAIL_MESSAGE("EVP_Digest failed");

    format_hex(md_value, md_len, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "c50b6bea7a0c35a3701aa22a78cb24860b8e7a6f9bfae597c6bbe746ad6426e0",
        hexdigest);
}

/*
 * 4 workers
 *
 * Workers process blocks with index % 4 == id:
 * - worker 0: block 0, block 4
 * - worker 1: block 1, block 5
 * - worker 2: block 2, block 6
 * - worker 3: block 3, block 7
 */

void test_4_aligned_full()
{
    int err = 0;

    cfg = config_new(DIGEST_NAME, BLOCK_SIZE, 4);
    assert(cfg);

    struct worker ws[4];

    for (int i = 0; i < 4; i++)
        worker_init(&ws[i], i, cfg);

    for (int i = 0; i < BLOCKS; i++) {
        struct block *b = block_new(i, cfg->block_size, data[i]);
        assert(worker_update(&ws[i % 4], b) == 0);
    }

    for (int i = 0; i < 4; i++)
        worker_final(&ws[i], BLOCKS * cfg->block_size);

    /* Compute root hash. */
    if (!EVP_DigestInit_ex(ctx, cfg->md, NULL))
        TEST_FAIL_MESSAGE("EVP_DigestInit_ex failed");

    for (int i = 0; i < 4; i++) {
        worker_digest(&ws[i], md_value, &md_len);
        if (!EVP_DigestUpdate(ctx, md_value, md_len)) {
            if (err == 0)
                err = ENOMEM;
        }
    }

    if (err)
        TEST_FAIL_MESSAGE("EVP_DigestUpdate failed");

    if (!EVP_DigestFinal_ex(ctx, md_value, &md_len))
        TEST_FAIL_MESSAGE("EVP_DigestFinal_ex failed");

    for (int i = 0; i < 4; i++)
        worker_destroy(&ws[i]);

    format_hex(md_value, md_len, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "600ee3c494fd4f38abf71fe92a1550b502978c3dbf07c2ff2a0c4496d69115b7",
        hexdigest);
}

void test_4_aligned_sparse()
{
    int err = 0;
    cfg = config_new(DIGEST_NAME, BLOCK_SIZE, 4);
    assert(cfg);

    struct worker ws[4];

    for (int i = 0; i < 4; i++)
        worker_init(&ws[i], i, cfg);

    /* Skipping all blocks... */

    for (int i = 0; i < 4; i++)
        worker_final(&ws[i], BLOCKS * cfg->block_size);

    /* Compute root hash. */
    if (!EVP_DigestInit_ex(ctx, cfg->md, NULL))
        TEST_FAIL_MESSAGE("EVP_DigestInit_ex failed");

    for (int i = 0; i < 4; i++) {
        worker_digest(&ws[i], md_value, &md_len);
        if (!EVP_DigestUpdate(ctx, md_value, md_len)) {
            if (err == 0)
                err = ENOMEM;
        }
    }

    if (err)
        TEST_FAIL_MESSAGE("EVP_DigestUpdate failed");

    if (!EVP_DigestFinal_ex(ctx, md_value, &md_len))
        TEST_FAIL_MESSAGE("EVP_DigestFinal_ex failed");

    for (int i = 0; i < 4; i++)
        worker_destroy(&ws[i]);

    format_hex(md_value, md_len, hexdigest);
    TEST_ASSERT_EQUAL_STRING(
        "823d6ac7d26b7768abfbd2051a6bb167937043e884bac39ea8da31bae7bf5ace",
        hexdigest);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_1_aligned_full);
    RUN_TEST(test_1_aligned_sparse);

    RUN_TEST(test_4_aligned_full);
    RUN_TEST(test_4_aligned_sparse);

    return UNITY_END();
}
