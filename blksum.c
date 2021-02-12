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

#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/evp.h>
#include "blkhash.h"

size_t block_size = 1024*1024;
const char *digest_name;
const char *filename;

int main(int argc, char *argv[])
{
    int fd;
    struct blkhash *h;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len, i;
    bool done = false;

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

    while (!done) {
        int n = blkhash_read(h, fd);
        if (n < 0) {
            perror("blkhash_read");
            exit(1);
        }
        if (n == 0) {
            done = true;
        }
    }

    blkhash_final(h, md_value, &md_len);
    blkhash_free(h);

    for (i = 0; i < md_len; i++)
        printf("%02x", md_value[i]);

    printf("  %s\n", filename);

    exit(0);
}
