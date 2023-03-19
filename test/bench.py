# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import json
import os
import subprocess

ROOT_DIR = os.path.dirname(os.path.dirname(__file__))
BLKHASH_BENCH = os.path.join(ROOT_DIR, "build", "test", "blkhash-bench")

KiB = 1 << 10
MiB = 1 << 20
GiB = 1 << 30
TiB = 1 << 40


def blkhash(input_type, input_size, digest_name=None, threads=None):
    cmd = [
        BLKHASH_BENCH,
        f"--input-type={input_type}",
        f"--input-size={input_size}",
    ]
    if digest_name:
        cmd.append(f"--digest-name={digest_name}")
    if threads:
        cmd.append(f"--threads={threads}")
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    hsize = format_humansize(r["input-size"])
    hrate = format_humansize(r["throughput"])
    print(f"{r['threads']:>2} threads: {hsize} in {r['elapsed']:.3f} s ({hrate}/s)")


def format_humansize(n):
    for unit in ("bytes", "KiB", "MiB", "GiB", "TiB"):
        if n < KiB:
            break
        n /= KiB
    return "{:.{precision}f} {}".format(n, unit, precision=0 if unit == "bytes" else 2)
