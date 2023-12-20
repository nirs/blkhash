#!/usr/bin/env python3

import os
import bench

base = "/data/tmp/blksum"
digest = "sha256"
prefix = f"blksum-{digest}-nbd"
formats = ["raw", "qcow2"]
images = ["20p", "40p", "80p"]

args = bench.parse_args()

bench.build(nbd="enabled")

for format in formats:
    files = []

    for image in images:
        filename = os.path.join(base, f"{image}.{format}")
        output = f"{prefix}-{format}-{image}.json"
        bench.blksum(
            filename,
            digest_name=digest,
            output=output,
            max_threads=args.max_threads,
            cache=True,
            cool_down=args.cool_down,
            runs=args.runs,
        )
        files.append(output)

    bench.plot_blksum(
        *files,
        title=f"blksum {digest} nbd {format}",
        output=f"{prefix}-{format}.png",
    )
