#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
"""
Generate plot from blkhash benchmarks results.
"""

import sys
import json
import os

import matplotlib.pyplot as plt

import bench
from units import *

if len(sys.argv) != 2:
    sys.exit(f"Usage: {sys.argv[0]} FILENAME")

filename = sys.argv[1]
with open(filename) as f:
    results = json.load(f)

plt.style.use(["test/paper.mplstyle"])
fig, ax = plt.subplots(layout="constrained")

for data in results["data"]:
    x = [r["threads"] for r in data["runs"]]
    y = [r["throughput"] / GiB for r in data["runs"]]
    ax.plot(
        x,
        y,
        linewidth=data.get("linewidth"),
        marker=data.get("marker", "s"),
        markersize=data.get("markersize"),
        zorder=data.get("zorder"),
        label=data["name"],
    )

title = results["test-name"]
if results["host-name"]:
    title += f" ({results['host-name']})"
fig.suptitle(title)
ax.legend()
ax.grid(**results.get("grid", {}))
ax.set_xlabel(results["xlabel"])
ax.set_xscale(results["xscale"])
#ax.set_xticks([r["threads"] for r in results["data"][0]["runs"]])
ax.set_ylabel(results["ylabel"])
ax.set_yscale(results["yscale"])
ax.set_ylim(bottom=0)

basename, _ = os.path.splitext(filename)
plt.savefig(basename + ".eps", format="eps")
