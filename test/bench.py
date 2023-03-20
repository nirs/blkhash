# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import json
import os
import subprocess

# We pass the build directory from meson.build to support running the tests
# during rpmbuild, when the build directory is not located in the same place.
build_dir = os.environ.get("BUILD_DIR")
if build_dir is None:
    root_dir = os.path.dirname(os.path.dirname(__file__))
    build_dir = os.path.join(root_dir, "build", "test")

BLKHASH_BENCH = os.path.join(build_dir, "blkhash-bench")
OPENSSL_BENCH = os.path.join(build_dir, "openssl-bench")

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
    return r


def openssl(input_size, digest_name=None):
    cmd = [OPENSSL_BENCH, f"--input-size={input_size}"]
    if digest_name:
        cmd.append(f"--digest-name={digest_name}")
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    hsize = format_humansize(r["input-size"])
    hrate = format_humansize(r["throughput"])
    print(f"{r['threads']:>2} threads: {hsize} in {r['elapsed']:.3f} s ({hrate}/s)")
    return r


def format_humansize(n):
    for unit in ("bytes", "KiB", "MiB", "GiB", "TiB"):
        if n < KiB:
            break
        n /= KiB
    return "{:.{precision}f} {}".format(n, unit, precision=0 if unit == "bytes" else 2)
