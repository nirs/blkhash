#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

"""
blksum reference implementaion.

This is a simple and slow implementaion for verifying the C
implementation. It does not use any optimzation and support only raw
images.

Usage:

    ./blksum.py DIGESTNAME FILENAME

"""

import hashlib
import os
import sys

# Values must match the compiled valeus in blksum.c.
# Changing these will change the computed checksum.
BLOCK_SIZE = 64 * 1024
SEGMENT_SIZE = 128 * 1024 * 1024


def checksum(digestname, filename):
    """
    Compute block checksum for raw image.

    Compute root hash for segments:

        H( H(segment 1) + H(segment 2) ... H(segment N)) )

    H(segment N) is:

        H( H(block 1) + H(block 2) ... H(block M)) )
    """
    size = os.path.getsize(filename)
    root = hashlib.new(digestname)

    with open(filename, "rb") as f:
        for offset in range(0, size, SEGMENT_SIZE):
            # The last segment may be shorter.
            length = min(SEGMENT_SIZE, size - offset)

            # Compute segment digest.
            segment = hashlib.new(digestname)

            while length:
                # Compute block digest.

                # The last block of the last segment may be shorter.
                block_size = min(BLOCK_SIZE, length)
                block = hashlib.new(digestname)

                # Read and hash entire block.
                read = 0
                while read < block_size:
                    data = f.read(block_size - read)
                    if not data:
                        raise RuntimeError(
                            f"Unexpected end of file at offset {f.tell()}, "
                            f"expected {size} bytes")

                    block.update(data)
                    read += len(data)

                # Add block hash to segment.
                segment.update(block.digest())
                length -= block_size

            # Add segment hash to root hash.
            root.update(segment.digest())

    return root.hexdigest()


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: blksum.py DIGESTNAME FILENAME", file=sys.stderr)
        sys.exit(1)

    digestname = sys.argv[1]
    filename = sys.argv[2]
    blksum = checksum(digestname, filename)

    print(f"{blksum}  {filename}")
