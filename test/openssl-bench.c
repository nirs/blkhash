// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <getopt.h>
#include <string.h>

#include <openssl/evp.h>

#include "benchmark.h"
#include "util.h"

static const char *digest_name = "sha256";
static int timeout_seconds = 1;
static int64_t input_size = 0;
static int read_size = 1 * MiB;
static unsigned char *buffer;

static const char *short_options = ":hd:T:s:r:";

static struct option long_options[] = {
   {"help",             no_argument,        0,  'h'},
   {"digest-name",      required_argument,  0,  'd'},
   {"timeout-seconds",  required_argument,  0,  'T'},
   {"input-size",       required_argument,  0,  's'},
   {"read-size",        required_argument,  0,  'r'},
   {0,              0,                  0,  0},
};

static void usage(int code)
{
    fputs(
        "\n"
        "Benchmark openssl\n"
        "\n"
        "    openssl-bench [-d DIGEST|--digest-name=DIGEST]\n"
        "                  [-T N|--timeout-seconds=N]\n"
        "                  [-s N|--input-size N]\n"
        "                  [-r N|--read-size N] [-h|--help]\n"
        "\n",
        stderr);

    exit(code);
}

static void parse_options(int argc, char *argv[])
{
    const char *optname;
    int c;

    /* Silence getopt_long error messages. */
    opterr = 0;

    while (1) {
        optname = argv[optind];
        c = getopt_long(argc, argv, short_options, long_options, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 'h':
            usage(0);
            break;
        case 'd':
            digest_name = optarg;
            break;
        case 'T':
            timeout_seconds = parse_seconds(optname, optarg);
            break;
        case 's':
            input_size = parse_size(optname, optarg);
            break;
        case 'r':
            read_size = parse_size(optname, optarg);
            break;
        case ':':
            FAILF("Option %s requires an argument", optname);
            break;
        case '?':
        default:
            FAILF("Invalid option: %s", optname);
        }
    }
}

static int64_t update_by_size(EVP_MD_CTX *ctx)
{
    int64_t todo = input_size;

    while (todo > read_size) {
        if (!EVP_DigestUpdate(ctx, buffer, read_size))
            FAIL("EVP_DigestUpdate");
        todo -= read_size;
    }

    if (todo > 0) {
        if (!EVP_DigestUpdate(ctx, buffer, todo))
            FAIL("EVP_DigestUpdate");
    }

    return input_size;
}

static int64_t update_until_timeout(EVP_MD_CTX *ctx)
{
    int64_t done = 0;

    do {
        if (!EVP_DigestUpdate(ctx, buffer, read_size))
            FAIL("EVP_DigestUpdate");
        done += read_size;
    } while (timer_is_running);

    return done;
}

int main(int argc, char *argv[])
{
    int64_t start, elapsed;
    const EVP_MD *md;
    EVP_MD_CTX *ctx;
    unsigned char res[EVP_MAX_MD_SIZE];
    char res_hex[EVP_MAX_MD_SIZE * 2 + 1];
    unsigned int len;
    int64_t total_size;
    double seconds;

    parse_options(argc, argv);

    buffer = malloc(read_size);
    if (buffer == NULL)
        FAIL("malloc");

    memset(buffer, 0x55, read_size);

    if (input_size == 0)
        start_timer(timeout_seconds);

    start = gettime();

    md = lookup_digest(digest_name);
    if (md == NULL)
        FAIL("lookup_digest");

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        FAIL("EVP_MD_CTX_new");

    if (!EVP_DigestInit_ex(ctx, md, NULL))
        FAIL("EVP_DigestInit_ex");

    if (input_size)
        total_size = update_by_size(ctx);
    else
        total_size = update_until_timeout(ctx);

    if (!EVP_DigestFinal_ex(ctx, res, &len))
        FAIL("EVP_DigestFinal_ex");

    EVP_MD_CTX_free(ctx);

    elapsed = gettime() - start;
    seconds = elapsed / 1e6;
    format_hex(res, len, res_hex);

    printf("{\n");
    printf("  \"input-type\": \"data\",\n");
    printf("  \"digest-name\": \"%s\",\n", digest_name);
    printf("  \"timeout-seconds\": %d,\n", timeout_seconds);
    printf("  \"input-size\": %" PRIi64 ",\n", input_size);
    printf("  \"read-size\": %d,\n", read_size);
    printf("  \"threads\": 1,\n");
    printf("  \"total-size\": %" PRIi64 ",\n", total_size);
    printf("  \"elapsed\": %.3f,\n", seconds);
    printf("  \"throughput\": %" PRIi64 ",\n", (int64_t)(total_size / seconds));
    printf("  \"checksum\": \"%s\"\n", res_hex);
    printf("}\n");

    free(buffer);
}
