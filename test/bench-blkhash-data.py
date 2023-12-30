#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

results = bench.results(
    f"blkhash {args.digest_name} - non zero",
    host_name=args.host_name,
)
results["grid"] = {"axis": "x"}

print(f"\nblkhash-bench --digest-name {args.digest_name} --input-type data\n")

runs = []
results["data"].append({"name": f"blk-{args.digest_name}", "runs": runs})

for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name=args.digest_name,
        threads=n,
        read_size=args.read_size,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name {args.digest_name} --input-type data --aio\n")

runs = []
results["data"].append({"name": f"blk-{args.digest_name} aio", "runs": runs})

for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        digest_name=args.digest_name,
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
