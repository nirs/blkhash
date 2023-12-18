#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

results = bench.results(
    f"blkhash {args.digest_name} - all zeros",
    host_name=args.host_name,
)
results["grid"] = {"axis": "x"}

print(f"\nblkhash-bench --digest-name {args.digest_name} --input-type zero\n")

runs = []
results["data"].append({"name": f"blk-{args.digest_name}", "runs": runs})

for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "zero",
        digest_name=args.digest_name,
        threads=n,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

print(f"\nblkhash-bench --digest-name {args.digest_name} --input-type zero --aio\n")

runs = []
results["data"].append({"name": f"blk-{args.digest_name} aio", "runs": runs})

for n in bench.threads(args.max_threads):
    r = bench.blkhash(
        "zero",
        digest_name=args.digest_name,
        threads=n,
        aio=True,
        timeout_seconds=args.timeout,
        cool_down=args.cool_down,
    )
    runs.append(r)

if args.output:
    bench.write(results, args.output)
