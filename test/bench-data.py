#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()
results = bench.results(
    "blkhash throughput - non zero",
    host_name=args.host_name,
)
results["grid"] = {"axis": "x"}

print(f"\nblkhash-bench --digest-name {bench.DIGEST} --input-type data\n")

runs = []
results["data"].append({"name": "blkhash", "runs": runs})

for n in bench.threads(args.max_threads):
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

for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "data",
        threads=n,
        aio=True,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

if args.output:
    bench.write(results, args.output)
