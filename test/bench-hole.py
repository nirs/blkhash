#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()
results = bench.results(
    "blkhash throughput - unallocated",
    host_name=args.host_name,
)
results["grid"] = {"axis": "x"}

print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type hole\n")

runs = []
results["data"].append({"name": "blkhash", "runs": runs})

for n in bench.threads():
    r = bench.blkhash(
        "hole",
        threads=n,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

if args.output:
    bench.write(results, args.output)
