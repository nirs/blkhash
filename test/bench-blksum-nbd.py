#!/usr/bin/env python3

import os
import bench

args = bench.parse_args()

formats = ["raw", "qcow2"]
images = ["20p", "40p", "80p"]

bench.build(nbd="enabled", blake3="enabled")

outdir = os.path.join(args.out_dir, "blksum")
os.makedirs(outdir, exist_ok=True)

for format in formats:
    files = []
    prefix = f"blksum-{args.digest_name}-nbd-{format}-r{args.read_size}-b{args.block_size}"

    for image in images:
        filename = os.path.join(args.image_dir, f"{image}.{format}")
        output = os.path.join(outdir, f"{prefix}-{image}.json")
        bench.blksum(
            filename,
            digest_name=args.digest_name,
            queue_depth=args.queue_depth,
            read_size=args.read_size,
            block_size=args.block_size,
            output=output,
            max_threads=args.max_threads,
            cache=True,
            image_cached=True,
            cool_down=args.cool_down,
            runs=args.runs,
        )
        files.append(output)

    bench.plot_blksum(
        *files,
        title=f"blksum {args.digest_name} nbd {format}",
        output=os.path.join(outdir, f"{prefix}.png"),
    )
