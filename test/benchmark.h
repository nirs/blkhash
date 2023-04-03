// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define FAILF(fmt, ...) do { \
    fprintf(stderr, fmt "\n", ## __VA_ARGS__); \
    exit(EXIT_FAILURE); \
} while (0)

#define FAIL(msg) do { \
    perror(msg); \
    exit(EXIT_FAILURE); \
} while (0)

#define MAX_THREADS 128

enum input_type {DATA, ZERO, HOLE};

extern volatile sig_atomic_t timer_is_running;

const char *type_name(enum input_type type);
int parse_type(const char *name, const char *arg);
int parse_seconds(const char *name, const char *arg);
int parse_threads(const char *name, const char *arg);
int64_t parse_size(const char *name, const char *arg);

void start_timer(int seconds);

#endif /* BENCHMARK_H */
