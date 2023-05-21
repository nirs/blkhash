# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import json
import os
import subprocess
import time

from units import *

# We pass the build directory from meson.build to support running the tests
# during rpmbuild, when the build directory is not located in the same place.
build_dir = os.environ.get("BUILD_DIR")
if build_dir is None:
    root_dir = os.path.dirname(os.path.dirname(__file__))
    build_dir = os.path.join(root_dir, "build", "test")

BLKHASH_BENCH = os.path.join(build_dir, "blkhash-bench")
OPENSSL_BENCH = os.path.join(build_dir, "openssl-bench")

DIGEST = "sha256"
STREAMS = 64
TIMEOUT = 0 if "QUICK" in os.environ else 2


def threads(limit=STREAMS):
    """
    Geneate powers of 2 up to limit. The value is also limited by the number of
    online cpus.

    For example on laptop with 12 cores:

        list(threads(limit=32)) -> [1, 2, 4, 8, 12]

    """
    online_cpus = os.sysconf("SC_NPROCESSORS_ONLN")
    n = 1
    while n <= limit:
        yield n
        if n >= online_cpus:
            break
        n = min(n * 2, online_cpus)


def blkhash(
    input_type,
    digest_name=DIGEST,
    timeout_seconds=TIMEOUT,
    input_size=None,
    aio=None,
    queue_depth=None,
    threads=4,
    streams=STREAMS,
):
    cmd = [
        BLKHASH_BENCH,
        f"--input-type={input_type}",
        f"--digest-name={digest_name}",
        f"--timeout-seconds={timeout_seconds}",
        f"--threads={threads}",
        f"--streams={streams}",
    ]

    if input_size:
        cmd.append(f"--input-size={input_size}")
    if aio:
        cmd.append("--aio")
        if queue_depth:
            cmd.append(f"--queue-depth={queue_depth}")

    _cool_down()
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    hsize = format_humansize(r["total-size"])
    hrate = format_humansize(r["throughput"])
    print(
        f"{r['threads']:>2} threads, {r['streams']} streams: "
        f"{hsize} in {r['elapsed']:.3f} s ({hrate}/s)"
    )
    return r


def openssl(
    digest_name=DIGEST,
    timeout_seconds=TIMEOUT,
    input_size=None,
    threads=1,
):
    cmd = [
        OPENSSL_BENCH,
        f"--digest-name={digest_name}",
        f"--timeout-seconds={timeout_seconds}",
        f"--threads={threads}",
    ]

    if input_size:
        cmd.append(f"--input-size={input_size}")

    _cool_down()
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    hsize = format_humansize(r["total-size"])
    hrate = format_humansize(r["throughput"])
    print(
        f"{r['threads']:>2} threads: {hsize} in {r['elapsed']:.3f} s ({hrate}/s)",
    )
    return r


def _cool_down():
    """
    For more consitent results on machines with frequency scalling, give the
    CPU time to cool down before runing the benchmark.
    """
    time.sleep(3 * TIMEOUT)


def format_humansize(n):
    for unit in ("bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"):
        if n < KiB:
            break
        n /= KiB
    return "{:.{precision}f} {}".format(n, unit, precision=0 if unit == "bytes" else 2)
