#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench

args = bench.parse_args()

results = bench.results(
    "blkhash null",
    host_name=args.host_name,
    yscale="log",
)

for input_type in "data", "zero", "hole":
    print(f"\nblkhash-bench --digest-name null --input-type {input_type}\n")
    runs = []
    results["data"].append({"name": input_type, "runs": runs})
    for n in bench.threads(args.max_threads):
        r = bench.blkhash(
            input_type,
            digest_name="null",
            threads=n,
            read_size=args.read_size,
            block_size=args.block_size,
            timeout_seconds=args.timeout,
            cool_down=args.cool_down,
        )
        runs.append(r)

    if input_type != "hole":
        print(f"\nblkhash-bench --digest-name null --input-type {input_type} --aio\n")
        runs = []
        results["data"].append({"name": f"{input_type} aio", "runs": runs})
        for n in bench.threads(args.max_threads):
            r = bench.blkhash(
                input_type,
                digest_name="null",
                threads=n,
                aio=True,
                queue_depth=args.queue_depth,
                read_size=args.read_size,
                block_size=args.block_size,
                timeout_seconds=args.timeout,
                cool_down=args.cool_down,
            )
            runs.append(r)

bench.write(results, f"blkhash-null-r{args.read_size}-b{args.block_size}.json")
