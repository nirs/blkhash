#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Show how image content affects blkhash throughput.

The most important optimization is detecting unallocated areas in the image and
processing them without reading anything from storage. A much smaller
optimization is detecting zeros in data read from storage, and eliminating the
computation. Include also single thread non-zero data using same algorithm for
reference.
"""

import os
import bench

args = bench.parse_args()

results = bench.results(
    f"blkhash {args.digest_name} - zero optimization",
    host_name=args.host_name,
    yscale="log",
)

results["grid"] = {"which": "both", "axis": "y"}

for input_type in ["data", "zero", "hole"]:
    print(f"\nblkhash-bench --digest-name {args.digest_name} --input-type {input_type} --aio\n")
    runs = []
    results["data"].append({"name": f"blk-{args.digest_name} {input_type}", "runs": runs})
    for n in bench.threads(args.max_threads):
        r = bench.blkhash(
            input_type,
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

print(f"\ndigest-bench --digest-name {args.digest_name}\n")

runs = []
results["data"].append({
    "name": args.digest_name,
    "marker": "",  # No marker
    "runs": runs,
})

r = bench.digest(
    digest_name=args.digest_name,
    threads=1,
    timeout_seconds=args.timeout,
    cool_down=args.cool_down,
)
runs.append(r)

# Add fake run to create a line instead of single point of data.
fake_run = r.copy()
fake_run["threads"] = bench.threads(args.max_threads)[-1]
runs.append(fake_run)

filename = os.path.join(
    args.out_dir,
    "blkhash",
    f"zero-optimization-{args.digest_name}-r{args.read_size}-b{args.block_size}.json",
)
bench.write(results, filename)
