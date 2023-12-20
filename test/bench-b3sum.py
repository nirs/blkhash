#!/usr/bin/env python3

import os
import bench

base = "/data/tmp/blksum"
prefix = "b3sum"
images = ["20p", "40p", "80p"]

args = bench.parse_args()

files = []

for image in images:
    filename = os.path.join(base, f"{image}.raw")
    output = f"{prefix}-{image}.json"
    bench.b3sum(
        filename,
        output=output,
        max_threads=args.max_threads,
        cool_down=args.cool_down,
        runs=args.runs,
    )
    files.append(output)

bench.plot_blksum(*files, title="b3sum", output=f"{prefix}.png")
