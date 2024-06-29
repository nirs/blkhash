#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Generate plot from blkhash benchmarks results.
"""

import argparse
import json
import os

import matplotlib.pyplot as plt
from matplotlib import ticker

import bench
from units import *

LABELS = {
    "gips": "Throughput (GiB/s)",
    "cpb": "Throughput (cpb)",
    "threads": "Number of threads",
}

p = argparse.ArgumentParser()
p.add_argument(
    "--key",
    choices=["gips", "cpb"],
    help="focus on throughput (gips) or cycles per byte (cpb)",
)
p.add_argument(
    "--target",
    choices=["web", "paper"],
    help="target for web (png) or for paper (eps)",
)
p.add_argument("filename", help="results filename")
args = p.parse_args()

if args.target == "web":
    file_format = "png"
elif args.target == "paper":
    file_format = "eps"
else:
    raise RuntimeError(f"Unsupported output: {args.output}")

with open(args.filename) as f:
    results = json.load(f)

plt.style.use([f"test/{args.target}.mplstyle"])
fig, ax = plt.subplots(layout="constrained")

for data in results["data"]:
    x = [r["threads"] for r in data["runs"]]
    y = [r[args.key] for r in data["runs"]]
    ax.plot(
        x,
        y,
        linewidth=data.get("linewidth"),
        marker=data.get("marker", "D"),
        markersize=data.get("markersize"),
        zorder=data.get("zorder"),
        label=data["name"],
    )

if args.target != "paper":
    title = results["test-name"]
    if results["host-name"]:
        title += f" ({results['host-name']})"
    fig.suptitle(title)

ax.legend()
ax.grid(**results.get("grid", {}))
ax.set_xlabel(LABELS[results["xlabel"]])
ax.set_xscale(results["xscale"])
ax.set_ylabel(LABELS[args.key])
ax.set_yscale(results["yscale"])
ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))

if results["yscale"] != "log":
    ax.set_ylim(bottom=0)

basename, _ = os.path.splitext(args.filename)
plt.savefig(f"{basename}-{args.key}.{file_format}", format=file_format)
