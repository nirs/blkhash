#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Generate plot from blksum benchmark results.
"""

import argparse
import sys
import json
import os

import matplotlib.pyplot as plt

import bench
from units import *

p = argparse.ArgumentParser()
p.add_argument("-o", "--output", help="Output file")
p.add_argument("--title", help="Plot Title")
p.add_argument("--log-scale", action="store_true", help="Use logarithmic scale")
p.add_argument("filenames", nargs="+", help="One or more input files")
args = p.parse_args()

plt.style.use(["test/web.mplstyle"])
fig, ax = plt.subplots(layout="constrained")

for filename in args.filenames:
    with open(filename) as f:
        results = json.load(f)

    # openssl results are from single thread and there is no "t" parameter.
    x = [r.get("parameters", {"t": "1"})["t"] for r in results["results"]]
    # Convert mean time to througput
    y = [results["size"] / r["mean"] / GiB for r in results["results"]]
    label = os.path.splitext(os.path.basename(filename))[0]
    extra = {}
    if len(x) == 1:
        extra["linewidth"] = 0
        extra["marker"] = "D"
    ax.plot(x, y, label=label, **extra)

fig.suptitle(args.title)
ax.legend()
ax.grid(which="both", axis="both")
ax.set_xlabel("Number of threads")
ax.set_ylabel("Throughput GiB/s")

if args.log_scale:
    ax.set_yscale("log")
else:
    ax.set_ylim(bottom=0)

if args.output:
    plt.savefig(args.output, format="png")
else:
    plt.show()
