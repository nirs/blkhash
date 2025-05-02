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

results = []

for filename in args.filenames:
    with open(filename) as f:
        data = json.load(f)

    label = data.get("label") or os.path.splitext(os.path.basename(filename))[0]

    x = [int(r["parameters"]["t"]) for r in data["results"]]

    # Convert mean time to throughput
    y = [data["size"] / r["mean"] / GiB for r in data["results"]]

    kwargs = {"label": label}
    if len(x) == 1:
        kwargs["linewidth"] = 0
        kwargs["marker"] = "D"

    results.append((x, y, kwargs))

for x, y, kwargs in results:
    ax.plot(x, y, **kwargs)

fig.suptitle(args.title)
ax.legend()
ax.grid(which="both", axis="both")
ax.set_xlabel("Number of threads")
ax.set_ylabel("Throughput GiB/s")
ax.set_xticks(results[0][0])

if args.log_scale:
    ax.set_yscale("log")
else:
    ax.set_ylim(bottom=0)

if args.output:
    plt.savefig(args.output, format="png")
else:
    plt.show()
