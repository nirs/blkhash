// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>

void format_hex(unsigned char *md, unsigned int len, char *s);
uint64_t gettime(void);
bool supports_direct_io(const char *filename);

#endif /* UTIL_H */
