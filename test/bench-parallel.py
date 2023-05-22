#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Show how number of threads affects throughput.

For parallel hasshing the best example is non-zero data, where this is the only
optimization. This is also the most important optimization since hashing image
data takes most of the time.

We show both the simple and async API since getting the most from parallel
hashing requires the async API. Show also sha256 for reference.
"""

import bench

args = bench.parse_args()
results = bench.results(
    "blkhash throughput - parallel hashing",
    host_name=args.host_name,
)

results["grid"] = {"axis": "x"}

print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type data\n")

runs = []
results["data"].append({"name": "blkhash", "runs": runs})

for n in bench.threads():
    r = bench.blkhash(
        "data",
        threads=n,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type data --aio\n")

runs = []
results["data"].append({"name": "blkhash-aio", "runs": runs})

for n in bench.threads():
    r = bench.blkhash(
        "data",
        threads=n,
        aio=True,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nopenssl-bench --digest-name {bench.DIGEST}\n")

runs = []
results["data"].append(
    {
        "name": "SHA256",
        "marker": "D",
        "linewidth": 0,
        "runs": runs,
    }
)

r = bench.openssl(
    threads=1,
    timeout_seconds=args.timeout,
    cool_down=args.cool_down,
)
runs.append(r)

if args.output:
    bench.write(results, args.output)
