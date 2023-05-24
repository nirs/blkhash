#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()
results = bench.results(
    "blkhash throughput - null digest",
    host_name=args.host_name,
    yscale="log",
)

for input_type in "data", "zero", "hole":
    print(f"\nblkhash-bench --digest-name null --input-type {input_type}\n")
    runs = []
    results["data"].append({"name": f"blkhash-{input_type}", "runs": runs})
    for n in bench.threads(args.max_threads):
        r = bench.blkhash(
            input_type,
            digest_name="null",
            threads=n,
            timeout_seconds=args.timeout,
            cool_down=args.cool_down,
        )
        runs.append(r)

    if input_type != "hole":
        print(f"\nblkhash-bench --digest-name null --input-type {input_type} --aio\n")
        runs = []
        results["data"].append({"name": f"blkhash-aio-{input_type}", "runs": runs})
        for n in bench.threads(args.max_threads):
            r = bench.blkhash(
                input_type,
                digest_name="null",
                threads=n,
                aio=True,
                timeout_seconds=args.timeout,
                cool_down=args.cool_down,
            )
            runs.append(r)

if args.output:
    bench.write(results, args.output)
