#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

"""
blkhash parallel reference implementaion.

This is a simple and slow implementaion for verifying the C
implementation. It does not use any optimzation and support only raw
images.

Usage:

    blkhash.py [-h] [-d DIGEST] filename

"""

import argparse
import collections
import hashlib
import queue
import struct
import threading

from functools import partial

# Values must match the compiled valeus in blksum.c.
# Changing these will change the computed checksum.
BLOCK_SIZE = 64 * 1024

# Values that do not affect te hash value.
THREADS = 4
READ_SIZE = 1024**2
QUEUE_DEPTH = 32


def main():
    p = argparse.ArgumentParser()
    p.add_argument(
        "-d",
        "--digest-name",
        default="sha256",
        help="Digest name (default sha256)",
    )
    p.add_argument(
        "-t",
        "--threads",
        type=int,
        default=THREADS,
        help=f"Number of threads (default {THREADS})",
    )
    p.add_argument(
        "-q",
        "--queue-depth",
        type=int,
        default=QUEUE_DEPTH,
        help=f"Number of block to queue (default {QUEUE_DEPTH})",
    )
    p.add_argument("filename", help="Filename to checksum")

    args = p.parse_args()
    blkhash = checksum(
        args.filename,
        args.digest_name,
        threads=args.threads,
        queue_depth=args.queue_depth,
    )
    print(f"{blkhash}  {args.filename}")


def checksum(
    filename,
    digest_name,
    block_size=BLOCK_SIZE,
    threads=THREADS,
    queue_depth=QUEUE_DEPTH,
):
    with open(filename, "rb") as f:
        h = Blkhash(
            digest_name,
            block_size=block_size,
            threads=threads,
            queue_depth=queue_depth,
        )
        for data in iter(partial(f.read, READ_SIZE), b""):
            h.update(data)
        return h.hexdigest()


class Blkhash:
    """
    Compute block checksum for raw image.

    The hash is:

        H( H(block 1) || H(block 2) ... H(block N) || length)

    """

    def __init__(
        self,
        digest_name,
        block_size=BLOCK_SIZE,
        threads=THREADS,
        queue_depth=QUEUE_DEPTH,
    ):
        assert threads > 0

        self.block_size = block_size
        self.queue_depth = queue_depth
        self.hashpool = HasherPool(digest_name, threads)
        self.futures = collections.deque()
        self.root = hashlib.new(digest_name)
        self.pending = None
        self.length = 0
        self.finalized = False

    def update(self, data):
        if self.finalized:
            raise RuntimeError("Hash finalized")

        self.length += len(data)

        with memoryview(data) as view:
            if self.pending:
                take = self.block_size - len(self.pending)
                self.pending += view[:take]
                view = view[take:]
                if len(self.pending) == self.block_size:
                    self._add_block(self.pending)
                    self.pending = None

            while len(view) >= self.block_size:
                self._add_block(view[: self.block_size])
                view = view[self.block_size :]

            if len(view):
                self.pending = bytearray(view)

    def hexdigest(self):
        self._finalize()
        return self.root.hexdigest()

    def _add_block(self, data):
        if len(self.futures) == self.queue_depth:
            block_digest = self.futures.popleft().result()
            self.root.update(block_digest)
        future = self.hashpool.hash(data)
        self.futures.append(future)
        while len(self.futures) and self.futures[0].done():
            block_digest = self.futures.popleft().result()
            self.root.update(block_digest)

    def _finalize(self):
        if self.finalized:
            return
        self.finalized = True
        if self.pending:
            self._add_block(self.pending)
            self.pending = None
        while len(self.futures):
            block_digest = self.futures.popleft().result()
            self.root.update(block_digest)

        data = struct.pack("<Q", self.length)
        self.root.update(data)

        self.hashpool.stop()


Work = collections.namedtuple("Work", "data,future")


class HasherPool:
    def __init__(self, digest_name, threads):
        self.hashers = []
        self.queue = queue.SimpleQueue()
        for i in range(threads):
            t = threading.Thread(
                target=self._hasher,
                args=(digest_name,),
                name=f"blkhash/{i}",
            )
            t.start()
            self.hashers.append(t)

    def hash(self, data):
        work = Work(data, Future())
        self.queue.put(work)
        return work.future

    def stop(self):
        for _ in self.hashers:
            self.queue.put(None)
        for t in self.hashers:
            t.join()

    def _hasher(self, digest_name):
        hasher = hashlib.new(digest_name).copy
        while True:
            work = self.queue.get()
            if work is None:
                break
            h = hasher()
            h.update(work.data)
            work.future.set_result(h.digest())
            work = None


class Future:
    def __init__(self):
        self._result = None
        self._done = threading.Event()

    def set_result(self, result):
        self._result = result
        self._done.set()

    def done(self):
        return self._done.is_set()

    def result(self):
        self._done.wait()
        return self._result


if __name__ == "__main__":
    main()
