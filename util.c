// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <stdio.h>

void format_hex(unsigned char *md, unsigned int len, char *s)
{
    for (int i = 0; i < len; i++) {
        snprintf(&s[i * 2], 3, "%02x", md[i]);
    }
    s[len * 2] = 0;
}
