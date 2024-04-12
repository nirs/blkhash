// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blkhash.h"
#include "blksum.h"
#include "src.h"

/* 16 is typically best, allow larger number for testing. */
#define MAX_QUEUE_DEPTH 128

/* Allow larger number for testing on big machines. */
#define MAX_THREADS 128

bool debug = false;
uint64_t started = 0;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t main_thread;
static bool failed;
static volatile sig_atomic_t terminated;

static struct options opt = {

    /* The default diget name, override with --digest. */
    .digest_name = "sha256",

    /*
     * Maximum read size in bytes. The curent value gives best
     * performance with i7-10850H when reading from fast NVMe. More
     * testing is needed with shared storage and different CPUs.
     */
    .read_size = 256 * KiB,

    /*
     * Maximum number of of inflight async reads.
     */
    .queue_depth = 16,

    /*
     * Smaller size is optimal for hashing and detecting holes.
     */
    .block_size = 64 * KiB,

    /*
     * Number of blkhash threads, does not change the hash value.
     */
    .threads = 4,

    /* Maximum size for extents call. */
    .extents_size = 1 * GiB,

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
    QUEUE_DEPTH=CHAR_MAX + 1,
    READ_SIZE,
    BLOCK_SIZE,
};

/* Start with ':' to enable detection of missing argument. */
static const char *short_options = ":hld:t:cp";

static struct option long_options[] = {
   {"help",         no_argument,        0,  'h'},
   {"list-digests", no_argument,        0,  'l'},
   {"digest",       required_argument,  0,  'd'},
   {"progress",     no_argument,        0,  'p'},
   {"cache",        no_argument,        0,  'c'},
   {"threads",      required_argument,  0,  't'},
   {"queue-depth",  required_argument,  0,  QUEUE_DEPTH},
   {"read-size",    required_argument,  0,  READ_SIZE},
   {"block-size",   required_argument,  0,  BLOCK_SIZE},
   {0,              0,                  0,  0}
};

static void usage(int code)
{
    fputs(
        "\n"
        "Compute message digest for disk images\n"
        "\n"
        "    blksum [-d DIGEST|--digest=DIGEST] [-p|--progress]\n"
        "           [-c|--cache] [-t N|--threads N] [--queue-depth=N]\n"
        "           [--read-size=N] [--block-size=N] [-l|--list-digests]\n"
        "           [-h|--help] [filename]\n"
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
        case 'p':
            opt.progress = !!isatty(fileno(stdout));
            break;
        case 'c':
            opt.cache = true;
            opt.flags |= USER_CACHE;
            break;
        case 't': {
            int value = parse_humansize(optarg);
            if (value == -EINVAL || value < 1 || value > MAX_THREADS)
                FAIL("Invalid value for option %s: '%s' (valid range 1-%d)",
                     optname, optarg, MAX_THREADS);

            opt.threads = value;
            break;
        }
        case QUEUE_DEPTH: {
            int value = parse_humansize(optarg);
            if (value == -EINVAL || value > MAX_QUEUE_DEPTH)
                FAIL("Invalid value for option %s: '%s' (valid range 1-%d)",
                     optname, optarg, MAX_QUEUE_DEPTH);

            opt.queue_depth = value;
            opt.flags |= USER_QUEUE_DEPTH;
            break;
        }
        case READ_SIZE: {
            int value = parse_humansize(optarg);
            if (value == -EINVAL)
                FAIL("Invalid value for option %s: '%s'", optname, optarg);

            opt.read_size = value;
            opt.flags |= USER_READ_SIZE;
            break;
        }
        case BLOCK_SIZE: {
            long page_size = sysconf(_SC_PAGE_SIZE);
            if (page_size == -1)
                page_size = 4 * KiB; /* Safe default for checkingb alignment. */

            int value = parse_humansize(optarg);
            if (value == -EINVAL)
                FAIL("Invalid value for option %s: '%s'", optname, optarg);

            if (opt.block_size % page_size)
                FAIL("Invalid block-size (%zu) is not a multiply of page size (%ld)",
                     opt.block_size, page_size);

            opt.block_size = value;
            break;
        }
        case ':':
            FAIL("Option %s requires an argument", optname);
            break;
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
        FAIL("read-size %ld is smaller than block size %ld",
             opt.read_size, opt.block_size);

    /* Parse arguments */

    if (optind < argc)
        opt.filename = argv[optind++];
}

static void handle_signal(int signum)
{
    assert(signum > 0);
    if (!terminated)
        terminated = signum;
}

static void setup_signals(void)
{
    sigset_t all;
    sigfillset(&all);

    struct sigaction act = {
        .sa_handler = handle_signal,
        .sa_mask = all,
    };

    if (sigaction(SIGINT, &act, NULL) != 0)
        FAIL_ERRNO("sigaction");

    if (sigaction(SIGTERM, &act, NULL) != 0)
        FAIL_ERRNO("sigaction");
}

bool running(void)
{
    pthread_mutex_lock(&lock);
    bool value = !(terminated || failed);
    pthread_mutex_unlock(&lock);
    return value;
}

void fail(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    pthread_mutex_lock(&lock);

    /* Ignore the failure if terminated, unless we run in debug mode. */
    if (!(failed || terminated) || debug) {
        failed = true;
        vfprintf(stderr, fmt, args);
        fflush(stderr);
    }

    pthread_mutex_unlock(&lock);

    va_end(args);

    /* If a worker thread failed, exit only the thread. All other threads
     * will exit when detecting that the application was failed. If the
     * main thread failed, exit the process. */

    if (pthread_equal(pthread_self(), main_thread))
        exit(EXIT_FAILURE);
    else
        pthread_exit(NULL);
}

static int compare(const void *p1, const void *p2)
{
   return strcmp(*(const char **)p1, *(const char **)p2);
}

void list_digests(void)
{
    // 20 digests expected.
    const char *names[40];
    size_t count;

    count = blkhash_digests(names, ARRAY_SIZE(names));

    qsort(names, count, sizeof(*names), compare);

    for (size_t i = 0; i < count; i++)
        puts(names[i]);

    exit(0);
}

int main(int argc, char *argv[])
{
    unsigned char md_value[BLKHASH_MAX_MD_SIZE];
    unsigned int md_len;
    char md_hex[BLKHASH_MAX_MD_SIZE * 2 + 1];

    main_thread = pthread_self();

    debug = getenv("BLKSUM_DEBUG") != NULL;

    if (debug)
        started = gettime();

    parse_options(argc, argv);

    setup_signals();

    if (opt.filename) {
        /* TODO: remove filename parameter */
        aio_checksum(opt.filename, &opt, md_value, &md_len);
    } else {
        struct src *s;
        s = open_pipe(STDIN_FILENO);
        checksum(s, &opt, md_value, &md_len);
        src_close(s);
    }

    pthread_mutex_lock(&lock);

    if (terminated) {
        /* Be quiet if user interrupted. */
        if (terminated != SIGINT) {
            ERROR("Terminated by signal %d", terminated);
            fflush(stderr);
        }

        /* Terminate by termination signal. */
        signal(terminated, SIG_DFL);
        pthread_mutex_unlock(&lock);
        raise(terminated);
    }

    if (failed) {
        /* The failing thread already reported the error. */
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_unlock(&lock);

    format_hex(md_value, md_len, md_hex);
    printf("%s  %s\n", md_hex, opt.filename ? opt.filename : "-");

    return 0;
}
