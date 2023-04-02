// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BENCHMARK_H
#define BENCHMARK_H

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

enum input_type {DATA, ZERO, HOLE};

const char *type_name(enum input_type type);
int parse_type(const char *name, const char *arg);
double parse_seconds(const char *name, const char *arg);
int parse_count(const char *name, const char *arg);
int64_t parse_size(const char *name, const char *arg);

#endif /* BENCHMARK_H */
