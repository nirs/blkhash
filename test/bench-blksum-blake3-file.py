#!/usr/bin/env python3

import os
import bench

base = "/data/tmp/blksum"
digest = "blake3"
prefix = f"blksum-{digest}-file"
images = ["20p", "40p", "80p"]

args = bench.parse_args()

bench.build(nbd="disabled", blake3="enabled")

files = []

for image in images:
    filename = os.path.join(base, f"{image}.raw")
    output = f"{prefix}-raw-{image}.json"
    bench.blksum(
        filename,
        digest_name=digest,
        output=output,
        max_threads=args.max_threads,
        cool_down=args.cool_down,
        runs=args.runs,
    )
    files.append(output)

bench.plot_blksum(
    *files,
    title=f"blksum {digest} file raw",
    output=f"{prefix}-raw.png",
)
