#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Show how image content affects blkhash throughput.

The most important optimization is detecting unallocated areas in the image and
processing them withotu reading anyting from storage. A much smaller
optimization is detecting zeros in data read from storage, and eliminating the
computation. Include also non-zero data and sha256 for reference.
"""

import bench

args = bench.parse_args()
results = bench.results(
    "blkhash throughput - zero optimization",
    host_name=args.host_name,
    yscale="log",
)

results["grid"] = {"which": "both", "axis": "y"}

for input_type in ["data", "zero", "hole"]:
    print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type {input_type} --aio\n")
    runs = []
    results["data"].append({"name": f"blkhash-{input_type}", "runs": runs})
    for n in bench.threads():
        r = bench.blkhash(
            input_type,
            threads=n,
            aio=True,
            timeout_seconds=args.timeout,
            cool_down=args.cool_down,
        )
        runs.append(r)

print(f"\nopenssl-bench --digest-name {bench.DIGEST}\n")

runs = []
results["data"].append({
    "name": "SHA256",
    "marker": "D",
    "linewidth": 0,
    "runs": runs,
})

r = bench.openssl(
    threads=1,
    timeout_seconds=args.timeout,
    cool_down=args.cool_down,
)
runs.append(r)

if args.output:
    bench.write(results, args.output)
