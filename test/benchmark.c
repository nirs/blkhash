// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "benchmark.h"
#include "util.h"

const char *type_name(enum input_type type)
{
    switch (type) {
        case DATA: return "data";
        case ZERO: return "zero";
        case HOLE: return "hole";
    default:
        return "unknown";
    }
}

int parse_type(const char *name, const char *arg)
{
    if (strcmp(arg, "data") == 0)
        return DATA;

    if (strcmp(arg, "zero") == 0)
        return ZERO;

    if (strcmp(arg, "hole") == 0)
        return HOLE;

    FAILF("Invalid value for option %s: '%s'", name, arg);
}

double parse_seconds(const char *name, const char *arg)
{
    char *end;
    double value;

    value = strtod(arg, &end);
    if (*end != '\0' || value < 0.0) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}

int parse_count(const char *name, const char *arg)
{
    char *end;
    long value;

    value = strtol(arg, &end, 10);
    if (*end != '\0' || value < 1) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}

int64_t parse_size(const char *name, const char *arg)
{
    int64_t value;

    value = parse_humansize(arg);
    if (value < 1 || value == -EINVAL) {
        FAILF("Invalid value for option %s: '%s'", name, arg);
    }

    return value;
}
