// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src.h"
#include "blkhash-config.h"

bool is_nbd_uri(const char *s)
{
    /*
     * libbnd supports more options like TLS and vsock, but I'm not sure
     * these are relevant to this tool.
     */
    return strncmp(s, "nbd://", 6) == 0 ||
           strncmp(s, "nbd+unix:///", 12) == 0;
}

struct src *open_src(const char *filename)
{
    if (is_nbd_uri(filename)) {
#ifdef HAVE_NBD
        return open_nbd(filename);
#else
        FAIL("NBD is not supported");
#endif
    }

    return open_file(filename);
}
