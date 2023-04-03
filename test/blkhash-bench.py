#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import os
import bench

STREAMS = 64
TIMEOUT = 0 if "QUICK" in os.environ else 1


def threads(streams):
    n = 1
    while n <= streams:
        yield n
        n *= 2


print("\nblkhash-bench --digest-name sha256 --input-type data\n")

for n in threads(STREAMS):
    bench.blkhash(
        "data",
        digest_name="sha256",
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )

print("\nblkhash-bench --digest-name sha256 --input-type zero\n")

for n in threads(STREAMS):
    bench.blkhash(
        "zero",
        digest_name="sha256",
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )

print("\nblkhash-bench --digest-name sha256 --input-type hole\n")

for n in threads(STREAMS):
    bench.blkhash(
        "hole",
        digest_name="sha256",
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )

print("\nblkhash-bench --digest-name null --input-type data\n")

for n in threads(STREAMS):
    bench.blkhash(
        "data",
        digest_name="null",
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )

print("\nblkhash-bench --digest-name null --input-type zero\n")

for n in threads(STREAMS):
    bench.blkhash(
        "zero",
        digest_name="null",
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )

print("\nblkhash-bench --digest-name null --input-type hole\n")

for n in threads(STREAMS):
    bench.blkhash(
        "hole",
        digest_name="null",
        timeout_seconds=TIMEOUT,
        threads=n,
        streams=STREAMS,
    )
