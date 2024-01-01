// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SECOND 1000000000UL
#define GiB (1UL<<30)
#define KiB (1UL<<10)

static unsigned buf[256 * KiB];
static bool drop;
static const char *filename;

static inline double elapsed(struct timespec *a, struct timespec *b)
{
    struct timespec res;

    res.tv_sec = a->tv_sec - b->tv_sec;
    res.tv_nsec = a->tv_nsec - b->tv_nsec;

    if (res.tv_nsec < 0) {
        res.tv_sec--;
        res.tv_nsec += SECOND;
    }

    return (double)res.tv_sec + (double)res.tv_nsec / SECOND;
}

static void cache(int fd, off_t size)
{
    struct timespec start;
    struct timespec stop;
    double seconds;
    double rate;
    off_t pos = 0;

    if (posix_fadvise(fd, 0, size, POSIX_FADV_WILLNEED)) {
        perror("posix_fadvise");
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (pos < size) {
        ssize_t nr;
        size_t count;

        count = MIN(size - pos, sizeof(buf));

        do {
            nr = pread(fd, buf, count, pos);
        } while (nr == -1 && errno == EINTR);

        if (nr < 0) {
            perror("pread");
            exit(EXIT_FAILURE);
        }

        pos += nr;
    }

    clock_gettime(CLOCK_MONOTONIC, &stop);

    seconds = elapsed(&stop, &start);
    rate = size / GiB / seconds;

    printf("%.3f GiB in %.3f seconds (%.3f GiB/s)\n", (double)size / GiB, seconds, rate);
}

static void uncache(int fd, off_t size)
{
    if (posix_fadvise(fd, 0, size, POSIX_FADV_DONTNEED)) {
        perror("posix_fadvise");
        exit(EXIT_FAILURE);
    }

    if (fsync(fd)) {
        perror("fsync");
        exit(EXIT_FAILURE);
    }
}

static const char *short_options = "hd";

static struct option long_options[] = {
   {"help",     no_argument,    0,  'h'},
   {"drop",     no_argument,    0,  'd'},
   {0,          0,              0,  0},
};

static void usage(int code)
{
    fputs(
        "\n"
        "Cache or drop image data.\n"
        "\n"
        "    cache [-d|--drop] filename\n"
        "\n",
        stderr
    );

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
            drop = true;
            break;
        case '?':
        default:
            fprintf(stderr, "Error: invalid option: %s\n", optname);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc) {
        fprintf(stderr, "Error: filename not specified\n");
        exit(EXIT_FAILURE);
    }

    filename = argv[optind++];
}

int main(int argc, char *argv[])
{
    struct stat st;
    int fd;

    parse_options(argc, argv);

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &st)) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }

    if (drop)
        uncache(fd, st.st_size);
    else
        cache(fd, st.st_size);

    close(fd);

    return 0;
}
