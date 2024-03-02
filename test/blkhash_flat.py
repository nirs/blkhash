#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

"""
blkhash reference implementaion.

This is a trivial implemention not supporting zero optimization or
multi-threading. It is usefful for verifying the C implemention.

Usage:

    blkhash.py [-h] [-d DIGEST_NAME] [-b BLOCK_SIZE] filename

"""

import argparse
import hashlib
import struct

from functools import partial

# Values must match the compiled valeus in blksum.c.
# Changing these will change the computed checksum.
BLOCK_SIZE = 64 * 1024

# Values that do not affect te hash value.
READ_SIZE = 1024**2


def main():
    p = argparse.ArgumentParser()
    p.add_argument(
        "-d",
        "--digest-name",
        default="sha256",
        help="Digest name (default sha256)",
    )
    p.add_argument(
        "-b",
        "--block-size",
        type=int,
        default=BLOCK_SIZE,
        help="Digest name (default sha256)",
    )
    p.add_argument("filename", help="Filename to checksum")

    args = p.parse_args()
    blkhash = checksum(args.filename, args.digest_name, block_size=args.block_size)
    print(f"{blkhash}  {args.filename}")


def checksum(filename, digest_name, block_size=BLOCK_SIZE):
    with open(filename, "rb") as f:
        h = Blkhash(digest_name, block_size=block_size)
        for data in iter(partial(f.read, READ_SIZE), b""):
            h.update(data)
        return h.hexdigest()


class Blkhash:
    """
    Compute block checksum for raw image.

    The hash is:

        H( H(block 1) || ... || H(block N) || length)

    """

    def __init__(self, digest_name, block_size=BLOCK_SIZE):
        self.digest_name = digest_name
        self.block_size = block_size
        self.outer_hash = hashlib.new(digest_name)
        self.pending = None
        self.length = 0
        self.finalized = False

    def update(self, data):
        assert not self.finalized
        with memoryview(data) as view:
            # Consume pending data.
            if self.pending:
                take = self.block_size - len(self.pending)
                self.pending += view[:take]
                view = view[take:]
                if len(self.pending) == self.block_size:
                    self._add_block(self.pending)
                    self.pending = None

            # Consume all full blocks.
            while len(view) >= self.block_size:
                self._add_block(view[: self.block_size])
                view = view[self.block_size :]

            # Store partial block pending.
            if len(view):
                self.pending = bytearray(view)

    def hexdigest(self):
        self._finalize()
        return self.outer_hash.hexdigest()

    def _add_block(self, data):
        block_hash = hashlib.new(self.digest_name, data).digest()
        self.outer_hash.update(block_hash)
        self.length += len(data)

    def _finalize(self):
        if self.finalized:
            return
        self.finalized = True
        if self.pending:
            self._add_block(self.pending)
            self.pending = None
        data = struct.pack("<Q", self.length)
        self.outer_hash.update(data)


if __name__ == "__main__":
    main()
