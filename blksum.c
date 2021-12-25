// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <openssl/evp.h>
#include "blkhash.h"
#include "blksum.h"
#include "util.h"

bool debug = false;

static struct options opt = {

    /*
     * Bigger size is optimal for reading, espcially when reading for
     * remote storage. Should be aligned to block size for best
     * performance.
     * TODO: Requires more testing with different storage.
     */
    .read_size = 2 * 1024 * 1024,

    /*
     * Smaller size is optimal for hashing and detecting holes. This
     * give best perforamnce when testing on zero image on local nvme
     * drive.
     * TODO: Requires more testing with different storage.
     */
    .block_size = 64 * 1024,

    /* Size of image segment. */
    .segment_size = 128 * 1024 * 1024,

    /* Number of worker threads to use. */
    .workers = 4,

    /* Avoid host page cache. */
    .nocache = false,
};

/* Start with ':' to enable detection of missing argument. */
static const char *short_options = ":w:n";

static struct option long_options[] = {
   {"workers", required_argument, 0,  'w'},
   {"nocache", no_argument,       0,  'n'},
   {0,         0,                 0,  0 }
};

static void parse_options(int argc, char *argv[])
{
    const char *optname;
    int c;

    /* Parse options. */

    /* Silence getopt_long error messages. */
    opterr = 0;

    while (1) {
        optname = argv[optind];
        c = getopt_long(argc, argv, short_options, long_options, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 'w': {
            char *end;
            opt.workers = strtol(optarg, &end, 10);
            if (*end != '\0' || end == optarg)
                FAIL("Invalid value for option %s: '%s'", optname, optarg);

            int online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
            if (opt.workers < 1 || opt.workers > online_cpus)
                FAIL("Invalid number of workers: %ld (1-%d)",
                     opt.workers, online_cpus);

            break;
        }
        case 'n':
            opt.nocache = true;
            break;
        case ':':
            FAIL("Option %s requires an argument", optname);
        case '?':
        default:
            FAIL("Invalid option: %s", optname);
        }
    }

    /* Parse arguments */

    if (optind == argc)
        FAIL("Usage: blksum [-w WORKERS] [-n] digestname [filename]");

    opt.digest_name = argv[optind++];

    if (optind < argc)
        opt.filename = argv[optind++];
}

void terminate(int signum)
{
    /* Kill child processes using same signal. */
    kill(0, signum);

    /* And exit with the expected exit code. */
    exit(signum + 128);
}

void setup_signals(void)
{
    sigset_t all;
    sigfillset(&all);

    struct sigaction act = {
        .sa_handler = terminate,
        .sa_mask = all,
    };

    if (sigaction(SIGINT, &act, NULL) != 0)
        FAIL_ERRNO("sigaction");

    if (sigaction(SIGTERM, &act, NULL) != 0)
        FAIL_ERRNO("sigaction");
}

int main(int argc, char *argv[])
{
    const EVP_MD *md;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    char md_hex[EVP_MAX_MD_SIZE * 2 + 1];

    debug = getenv("BLKSUM_DEBUG") != NULL;

    parse_options(argc, argv);

    md = EVP_get_digestbyname(opt.digest_name);
    if (md == NULL)
        FAIL("Unknown digest '%s'", opt.digest_name);

    setup_signals();

    if (opt.filename) {
        /* TODO: remove filename parameter */
        parallel_checksum(opt.filename, &opt, md_value);
    } else {
        struct src *s;
        s = open_pipe(STDIN_FILENO);
        simple_checksum(s, &opt, md_value);
        src_close(s);
    }

    format_hex(md_value, EVP_MD_size(md), md_hex);
    printf("%s  %s\n", md_hex, opt.filename ? opt.filename : "-");

    return 0;
}
