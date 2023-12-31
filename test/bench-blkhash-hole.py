#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

results = bench.results(
    f"blkhash {args.digest_name} - unallocated",
    host_name=args.host_name,
)
results["grid"] = {"axis": "x"}

print(f"\nblkhash-bench --digest-name {args.digest_name} --input-type hole\n")

runs = []
results["data"].append({"name": f"blk-{args.digest_name}", "runs": runs})

for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "hole",
        digest_name=args.digest_name,
        threads=n,
        block_size=args.block_size,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

bench.write(results, f"blkhash-hole-{args.digest_name}-r{args.read_size}-b{args.block_size}.json")
