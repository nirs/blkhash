#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Show how number of threads affects throughput.

For parallel hasshing the best example is non-zero data, where this is the only
optimization. This is also the most important optimization since hashing image
data takes most of the time.

We show both the simple and async API since getting the most from parallel
hashing requires the async API. Show also single thread result for same
algorihtm for reference.
"""

import bench

args = bench.parse_args()

results = bench.results(
    f"blkhash {args.digest_name} - parallel hashing",
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
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\ndigest-bench --digest-name {args.digest_name}\n")

runs = []
results["data"].append(
    {
        "name": args.digest_name,
        "marker": "D",
        "linewidth": 0,
        "runs": runs,
    }
)

r = bench.digest(
    digest_name=args.digest_name,
    threads=1,
    timeout_seconds=args.timeout,
    cool_down=args.cool_down,
)
runs.append(r)

if args.output:
    bench.write(results, args.output)
