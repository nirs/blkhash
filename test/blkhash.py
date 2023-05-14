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
import hashlib
import queue
import threading

from collections import namedtuple
from functools import partial

# Values must match the compiled valeus in blksum.c.
# Changing these will change the computed checksum.
BLOCK_SIZE = 64 * 1024
STREAMS = 64

# Values that do not affect te hash value.
THREADS = 4
QUEUE_SIZE = 16
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
        "-t",
        "--threads",
        type=int,
        default=THREADS,
        help=f"Number of threads (default {THREADS})",
    )
    p.add_argument(
        "-s",
        "--streams",
        type=int,
        default=STREAMS,
        help=f"Number of streams (default {STREAMS})",
    )
    p.add_argument("filename", help="Filename to checksum")

    args = p.parse_args()
    blkhash = checksum(
        args.filename,
        args.digest_name,
        threads=args.threads,
        streams=args.streams,
    )
    print(f"{blkhash}  {args.filename}")


def checksum(
    filename,
    digest_name,
    block_size=BLOCK_SIZE,
    threads=THREADS,
    streams=STREAMS,
):
    with open(filename, "rb") as f:
        h = Blkhash(
            digest_name,
            block_size=block_size,
            threads=threads,
            streams=streams,
        )
        for data in iter(partial(f.read, READ_SIZE), b""):
            h.update(data)
        return h.hexdigest()


class Blkhash:
    """
    Compute block checksum for raw image.

    Compute root hash for N streams:

        H( H(stream 0) || H(stream 1) ... H(stream N-1) )

    Blocks are handled by the stream matching:

        stream_index == block_index % N

    Example with 16 blocks image and 4 streams:

        H(stream 0) = H( H(block 0) || H(block 4) || H(block 8)  || H(block 12) )
        H(stream 1) = H( H(block 1) || H(block 5) || H(block 9)  || H(block 13) )
        H(stream 2) = H( H(block 2) || H(block 6) || H(block 10) || H(block 14) )
        H(stream 3) = H( H(block 3) || H(block 7) || H(block 11) || H(block 15) )

    We use T threads, each stream is mapped to thread matching:

        thread_index = stream_index % T

    Example with 2 threads:

        thread 0: stream 0, stream 2
        thread 1: stream 1, stream 3

    Example with 4 threads:

        thread 0: stream 0
        thread 1: stream 1
        thread 2: stream 2
        thread 3: stream 3

    """

    def __init__(
        self,
        digest_name,
        block_size=BLOCK_SIZE,
        threads=THREADS,
        streams=STREAMS,
    ):
        assert threads > 0
        assert threads <= streams

        self.digest_name = digest_name
        self.block_size = block_size
        self.streams = [hashlib.new(digest_name) for _ in range(streams)]
        self.workers = [Worker(digest_name) for _ in range(threads)]
        self.pending = None
        self.index = 0
        self.finalized = False

    def update(self, data):
        if self.finalized:
            raise RuntimeError("Hash finalized")

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
        root = hashlib.new(self.digest_name)
        for stream in self.streams:
            root.update(stream.digest())
        return root.hexdigest()

    def _add_block(self, data):
        stream_index = self.index % len(self.streams)
        worker_index = stream_index % len(self.workers)
        work = Work(data, self.streams[stream_index])
        self.workers[worker_index].submit(work)
        self.index += 1

    def _finalize(self):
        if not self.finalized:
            self.finalized = True
            if self.pending:
                self._add_block(self.pending)
                self.pending = None
            for worker in self.workers:
                worker.stop()
            for worker in self.workers:
                worker.wait()


Work = namedtuple("Work", "data, stream")


class Worker:
    def __init__(self, digest_name):
        self.digest_name = digest_name
        self.queue = queue.Queue(maxsize=QUEUE_SIZE)
        self.thread = threading.Thread(target=self.run)
        self.thread.daemon = True
        self.thread.start()

    def submit(self, work):
        self.queue.put(work)

    def stop(self):
        self.queue.put(None)

    def wait(self):
        self.thread.join()

    def run(self):
        while True:
            work = self.queue.get()
            if work is None:
                break

            digest = hashlib.new(self.digest_name, work.data).digest()
            work.stream.update(digest)


if __name__ == "__main__":
    main()
