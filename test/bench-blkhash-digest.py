#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

results = bench.results(
    "blkhash - digests",
    host_name=args.host_name,
)

print(f"\nblkhash-bench --digest-name blake3 --input-type data --aio\n")
runs = []
results["data"].append({"name": "blk-blake3", "runs": runs})
for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name="blake3",
        threads=n,
        aio=True,
        queue_depth=args.queue_depth,
        read_size=args.read_size,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name blake2b512 --input-type data --aio\n")
runs = []
results["data"].append({"name": "blk-blake2b512", "runs": runs})
for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name="blake2b512",
        threads=n,
        aio=True,
        queue_depth=args.queue_depth,
        read_size=args.read_size,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name sha512-256 --input-type data --aio\n")
runs = []
results["data"].append({"name": "blk-sha512-256", "runs": runs})
for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name="sha512-256",
        threads=n,
        aio=True,
        queue_depth=args.queue_depth,
        read_size=args.read_size,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name sha256 --input-type data --aio\n")
runs = []
results["data"].append({"name": "blk-sha256", "runs": runs})
for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name="sha256",
        threads=n,
        aio=True,
        queue_depth=args.queue_depth,
        read_size=args.read_size,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name sha3-256 --input-type data --aio\n")
runs = []
results["data"].append({"name": "blk-sha3-256", "runs": runs})
for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name="sha3-256",
        threads=n,
        aio=True,
        queue_depth=args.queue_depth,
        read_size=args.read_size,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

if args.output:
    bench.write(results, args.output)
