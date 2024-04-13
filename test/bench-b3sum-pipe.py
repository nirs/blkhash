#!/usr/bin/env python3

import os
import bench

args = bench.parse_args()

base = "/data/tmp/blksum"
prefix = "b3sum-pipe"
images = ["10p", "20p", "40p", "80p"]

outdir = os.path.join(args.out_dir, "b3sum")
os.makedirs(outdir, exist_ok=True)

files = []

for image in images:
    filename = os.path.join(args.image_dir, f"{image}.raw")
    output = os.path.join(outdir, f"{prefix}-{image}.json")
    bench.b3sum(
        filename,
        output=output,
        max_threads=args.max_threads,
        pipe=True,
        cool_down=args.cool_down,
        runs=args.runs,
        label=f"b3sum-{image}",
    )
    files.append(output)

bench.plot_blksum(
    *files,
    title="b3sum pipe",
    output=os.path.join(outdir, f"{prefix}.png"),
)
