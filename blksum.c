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

#define MAX_QUEUE_SIZE (8 * 1024 * 1024)
#define MAX_WORKERS 16

bool debug = false;
bool io_only = false;

static struct options opt = {

    /* The default diget name, override with --digest. */
    .digest_name = "sha256",

    /*
     * Maximum read size in bytes. The curent value gives best
     * performance with i7-10850H when reading from fast NVMe. More
     * testing is needed with shared storage and different CPUs.
     */
    .read_size = 256 * 1024,

    /*
     * Maximum number of bytes to queue for inflight async reads. With
     * the default read_size (256k) this will queue up to 8 inflight
     * requests with 256k length, or up to 16 inflight requests with 4k
     * length.
     */
    .queue_size = 2 * 1024 * 1024,

    /*
     * Smaller size is optimal for hashing and detecting holes.
     */
    .block_size = 64 * 1024,

    /* Size of image segment. */
    .segment_size = 128 * 1024 * 1024,

    /* Number of worker threads to use. This is the most important
     * configuration for best performance. */
    .workers = 4,

    /*
     * Use host page cache. This is may be faster, but is not correct
     * when using a block device connected to multiple hosts. Typically
     * it gives less consistent results. If not set, blksum uses direct
     * I/O when possible.
     */
    .cache = false,

    /*
     * The asynchronous I/O mode: "threads" (the default), "native" (Linux
     * only), and "io_uring" (Linux 5.1+). Use "native" on Linux since it gives
     * good performance with lower CPU usage in qemu-nbd.
     */
#if defined __linux__
    .aio = "native",
#else
    .aio = "threads",
#endif

    /* Show progress. */
    .progress = false,
};

enum {
    QUEUE_SIZE=CHAR_MAX + 1,
    READ_SIZE,
};

/* Start with ':' to enable detection of missing argument. */
static const char *short_options = ":hld:w:cp";

static struct option long_options[] = {
   {"help",         no_argument,        0,  'h'},
   {"list-digests", no_argument,        0,  'l'},
   {"digest",       required_argument,  0,  'd'},
   {"workers",      required_argument,  0,  'w'},
   {"progress",     no_argument,        0,  'p'},
   {"cache",        no_argument,        0,  'c'},
   {"queue-size",   required_argument,  0,  QUEUE_SIZE},
   {"read-size",    required_argument,  0,  READ_SIZE},
   {0,              0,                  0,  0}
};

static void usage(int code)
{
    fputs(
        "\n"
        "Compute message digest for disk images\n"
        "\n"
        "    blksum [-d DIGEST|--digest=DIGEST] [-w N|--workers=N]\n"
        "           [-p|--progress] [-c|--cache] [--queue-size=N]\n"
        "           [--read-size=N] [-l|--list-digests] [-h|--help]\n"
        "           [filename]\n"
        "\n"
        "Please read the blksum(1) manual page for more info.\n"
        "\n",
        stderr);

    exit(code);
}

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
        case 'h':
            usage(0);
            break;
        case 'l':
            list_digests();
            break;
        case 'd':
            opt.digest_name = optarg;
            break;
        case 'w': {
            char *end;
            opt.workers = strtol(optarg, &end, 10);
            if (*end != '\0' || end == optarg)
                FAIL("Invalid value for option %s: '%s'", optname, optarg);

            int online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
            int max_workers = online_cpus < MAX_WORKERS
                ? online_cpus : MAX_WORKERS;
            if (opt.workers < 1 || opt.workers > max_workers)
                FAIL("Invalid number of workers: %ld (1-%d)",
                     opt.workers, max_workers);

            opt.flags |= USER_WORKERS;
            break;
        }
        case 'p':
            opt.progress = true;
            break;
        case 'c':
            opt.cache = true;
            opt.flags |= USER_CACHE;
            break;
        case QUEUE_SIZE: {
            char *end;
            opt.queue_size = strtol(optarg, &end, 10);
            if (*end != '\0' || end == optarg)
                FAIL("Invalid value for option %s: '%s'", optname, optarg);

            opt.flags |= USER_QUEUE_SIZE;
            break;
        }
        case READ_SIZE: {
            char *end;
            opt.read_size = strtol(optarg, &end, 10);
            if (*end != '\0' || end == optarg)
                FAIL("Invalid value for option %s: '%s'", optname, optarg);

            opt.flags |= USER_READ_SIZE;
            break;
        }
        case ':':
            FAIL("Option %s requires an argument", optname);
        case '?':
        default:
            FAIL("Invalid option: %s", optname);
        }
    }

    /*
     * Validate arguments - must be done after all arguments are set,
     * since they depend on each other.
     */

    if (opt.read_size % opt.block_size)
        FAIL("Invalid read-size is not a multiply of block size (%ld)",
             opt.block_size);

    if (opt.read_size < opt.block_size)
        FAIL("read-size %ld is larger than block size %ld",
             opt.read_size, opt.block_size);

    if (opt.read_size > opt.queue_size)
        FAIL("read-size %ld is larger than queue-size %ld",
             opt.read_size, opt.queue_size);

    if (opt.queue_size > MAX_QUEUE_SIZE)
        FAIL("queue-size %ld is larger than maximum queue size %d",
             opt.queue_size, MAX_QUEUE_SIZE);

    /* Parse arguments */

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
    io_only = getenv("BLKSUM_IO_ONLY") != NULL;

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
