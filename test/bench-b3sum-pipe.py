#!/usr/bin/env python3

import os
import bench

args = bench.parse_args()

outdir = os.path.join(args.out_dir, "b3sum")
os.makedirs(outdir, exist_ok=True)

filename = os.path.join(args.image_dir, "20p.raw")
output = os.path.join(outdir, "b3sum-pipe.json")

bench.b3sum(
    filename,
    output=output,
    max_threads=args.max_threads,
    pipe=True,
    cool_down=args.cool_down,
    runs=args.runs,
    label="b3sum-20p",
)
