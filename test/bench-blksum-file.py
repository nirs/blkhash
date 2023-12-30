#!/usr/bin/env python3

import os
import bench

args = bench.parse_args()

base = "/data/tmp/blksum"
prefix = f"blksum-{args.digest_name}-file-r{args.read_size}-b{args.block_size}"
images = ["20p", "40p", "80p"]

bench.build(nbd="disabled", blake3="enabled")

files = []

for image in images:
    filename = os.path.join(base, f"{image}.raw")
    output = f"{prefix}-{image}.json"
    bench.blksum(
        filename,
        digest_name=args.digest_name,
        queue_depth=args.queue_depth,
        read_size=args.read_size,
        block_size=args.block_size,
        output=output,
        max_threads=args.max_threads,
        image_cached=True,
        cool_down=args.cool_down,
        runs=args.runs,
    )
    files.append(output)

bench.plot_blksum(
    *files,
    title=f"blksum {args.digest_name} file",
    output=f"{prefix}.png",
)
