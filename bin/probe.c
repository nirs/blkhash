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

#if defined __linux__
#include <sys/vfs.h>  /* For fstatfs */
#endif

#include <sys/param.h>  /* For __FreeBSD__ */
#if __FreeBSD__
#include <sys/mount.h>
#endif

#if __APPLE__
#include <sys/mount.h>
#endif

#include "blksum.h"

#define PROBE_SIZE 512

/* From qemu/block/qcow2.h */
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

static size_t read_probe(int fd, void *buf)
{
    ssize_t n;

    do {
        n = read(fd, buf, PROBE_SIZE);
    } while (n == -1 && errno == EINTR);

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

static void probe_format(int fd, struct file_info *fi)
{
    char buf[PROBE_SIZE];
    size_t len;

    len = read_probe(fd, &buf);

    if (is_qcow2(buf, len))
        fi->format = "qcow2";
    else
        fi->format = "raw";

    DEBUG("Probed image format: %s", fi->format);
}

static const char *fs_name(struct statfs *s)
{
#if defined __linux__
    /* Values documented in statfs(2). */
    switch (s->f_type) {
        case 0x73727279:
            return "btrfs";
        case 0xef53:
            return "ext4";
        case 0x65735546:
            return "fuse";
        case 0x6969:
            return "nfs";
        case 0x01021994:
            return "tmpfs";
        case 0x58465342:
            return "xfs";
    }
    DEBUG("Detected f_type=0x%0lx", s->f_type);
#endif
    return "other";
}

static void probe_fs_name(int fd, struct file_info *fi)
{
    struct statfs buf;

    /* Detecting file system type is optimization, don't fail. */
    if (fstatfs(fd, &buf) == -1) {
        DEBUG("Cannot probe file system type: %s", strerror(errno));
        return;
    }

    fi->fs_name = fs_name(&buf);

    DEBUG("Probed file system name: %s", fi->fs_name);
}

int probe_file(const char *path, struct file_info *fi)
{
    int fd;

    fd = open(path, O_RDONLY);
    if (fd == -1)
        FAIL_ERRNO("open");

    probe_format(fd, fi);
    probe_fs_name(fd, fi);

    close(fd);

    return 0;
}
