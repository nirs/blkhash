/*
 * blkhash - block based hash
 * Copyright Nir Soffer <nirsof@gmail.com>.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/evp.h>
#include "blkhash.h"

/*
 * Bigger size is optimal for reading, espcially when reading for remote
 * storage. Should be aligned to block size for best performance.
 * TODO: Requires more testing with different storage.
 */
const size_t read_size = 2 * 1024 * 1024;

/*
 * Smaller size is optimal for hashing and detecting holes. This give best
 * perforamnce when testing on zero image on local nvme drive.
 * TODO: Requires more testing with different storage.
 */
size_t block_size = 64 * 1024;

const char *digest_name;
const char *filename;

int main(int argc, char *argv[])
{
    int fd;
    struct blkhash *h;
    void *buf;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len, i;

    if (argc < 2) {
        fprintf(stderr, "Usage: blksum digestname [filename]");
        exit(2);
    }

    digest_name = argv[1];

    if (argv[2] != NULL) {
        filename = argv[2];

        fd = open(filename, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
    } else {
        filename = "-";
        fd = STDIN_FILENO;
    }

    h = blkhash_new(block_size, digest_name);
    if (h == NULL) {
        perror("blkhash_new");
        exit(1);
    }

    buf = malloc(read_size);
    if (buf == NULL) {
        perror("malloc");
        exit(1);
    }

    for (;;) {
        ssize_t n;

        /*
         * TODO: Should try to read full blocks to minimize copies in
         * blkhash_update().  Add read_full() helper that read complete buffer
         * until end of file.
         */
        do {
            n = read(fd, buf, read_size);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            perror("read");
            exit(1);
        }

        if (n == 0) {
            break;
        }

        blkhash_update(h, buf, n);
    }

    blkhash_final(h, md_value, &md_len);
    blkhash_free(h);

    for (i = 0; i < md_len; i++)
        printf("%02x", md_value[i]);

    printf("  %s\n", filename);

    exit(0);
}
