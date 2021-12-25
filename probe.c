// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blksum.h"

#define PROBE_SIZE 512

/* From qemu/blodk/qcow2.h */
#define QCOW_MAGIC (('Q' << 24) | ('F' << 16) | ('I' << 8) | 0xfb)

struct __attribute__((packed)) qcow2_header {
    uint32_t magic;
    uint32_t version;

    /*
     * The rest of the fields are not needed for probing, but padding to
     * the minimal header size is useful to behave like qemu-img.
     *
     * For version 2, the header is exactly 72 bytes in length. For
     * version 3 or higher, the header length is at least 104 bytes.
     */
    uint8_t padding[96];
};

static size_t read_probe(const char *path, void *buf)
{
    int fd;
    ssize_t n;

    fd = open(path, O_RDONLY);
    if (fd == -1)
        FAIL_ERRNO("open");

    do {
        n = read(fd, buf, PROBE_SIZE);
    } while (n == -1 && errno == EINTR);

    close(fd);

    if (n == -1)
        FAIL_ERRNO("read");

    return n;
}

static bool is_qcow2(const void *buf, size_t len)
{
    const struct qcow2_header *h = buf;

    return len >= sizeof(*h) &&
           ntohl(h->magic) == QCOW_MAGIC &&
           ntohl(h->version) >= 2;
}

const char *probe_format(const char *path)
{
    char buf[PROBE_SIZE];
    size_t len;
    const char *format;

    len = read_probe(path, &buf);

    if (is_qcow2(buf, len))
        format = "qcow2";
    else
        format = "raw";

    DEBUG("Probed image format: %s", format);

    return format;
}
