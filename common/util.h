// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>

#define KiB (1LL<<10)
#define MiB (1LL<<20)
#define GiB (1LL<<30)
#define TiB (1LL<<40)

/* May be defined in sys/param.h. */
#undef MIN
#undef MAX
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

void format_hex(unsigned char *md, unsigned int len, char *s);
char *humansize(int64_t bytes);
int64_t parse_humansize(const char *s);
uint64_t gettime(void);

#endif /* UTIL_H */
