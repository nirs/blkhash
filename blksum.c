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

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>
#include "blkhash.h"
#include "blksum.h"
#include "util.h"

bool debug = false;

struct options opt = {

    /*
     * Bigger size is optimal for reading, espcially when reading for
     * remote storage. Should be aligned to block size for best
     * performance.
     * TODO: Requires more testing with different storage.
     */
    .read_size = 2 * 1024 * 1024,

    /*
     * Smaller size is optimal for hashing and detecting holes. This
     * give best perforamnce when testing on zero image on local nvme
     * drive.
     * TODO: Requires more testing with different storage.
     */
    .block_size = 64 * 1024,

    /* Size of image segment. */
    .segment_size = 128 * 1024 * 1024,

    /* Number of worker threads to use. */
    .max_workers = 4,
};

int main(int argc, char *argv[])
{
    const char *filename;
    const EVP_MD *md;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    char md_hex[EVP_MAX_MD_SIZE * 2 + 1];

    if (argc < 2) {
        fprintf(stderr, "Usage: blksum digestname [filename]\n");
        exit(2);
    }

    debug = getenv("BLKSUM_DEBUG") != NULL;

    opt.digest_name = argv[1];

    md = EVP_get_digestbyname(opt.digest_name);
    if (md == NULL) {
        fprintf(stderr, "Unknown digest: %s\n", opt.digest_name);
        exit(2);
    }

    if (argv[2] != NULL) {
        filename = argv[2];
        parallel_checksum(filename, &opt, md_value);
    } else {
        struct src *s;
        filename = "-";
        s = open_pipe(STDIN_FILENO);
        simple_checksum(s, &opt, md_value);
        src_close(s);
    }

    format_hex(md_value, EVP_MD_size(md), md_hex);
    printf("%s  %s\n", md_hex, filename);

    return 0;
}
