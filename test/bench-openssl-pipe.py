#!/usr/bin/env python3

import os
import bench

args = bench.parse_args()

outdir = os.path.join(args.out_dir, "openssl")
os.makedirs(outdir, exist_ok=True)

filename = os.path.join(args.image_dir, "20p.raw")
output = os.path.join(outdir, f"openssl-pipe-{args.digest_name}.json")

bench.openssl(
    filename,
    digest_name=args.digest_name,
    output=output,
    max_threads=args.max_threads,
    pipe=True,
    cool_down=args.cool_down,
    runs=args.runs,
    label="openssl-20p",
)
