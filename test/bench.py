# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import argparse
import json
import os
import subprocess
import time

import host
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
TIMEOUT = 2
COOL_DOWN = 6


def parse_args():
    p = argparse.ArgumentParser("bench")
    p.add_argument(
        "--timeout",
        default=TIMEOUT,
        type=int,
        help=f"Number of seconds to run (default {TIMEOUT})",
    )
    p.add_argument(
        "--cool-down",
        default=COOL_DOWN,
        type=int,
        help=f"Number of seconds to wait between runs (default {COOL_DOWN})",
    )
    p.add_argument(
        "--host-name",
        help="Host name for graphs",
    )
    p.add_argument(
        "-o",
        "--output",
        help="Write results to specifed file (default no output)",
    )
    return p.parse_args()


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


def results(
    test_name,
    host_name=None,
    xlabel="Number of threads",
    xscale="linear",
    ylabel="Throughput GiB/s",
    yscale="linear",
):
    return {
        "test-name": test_name,
        "host-name": host_name,
        "xlabel": xlabel,
        "xscale": xscale,
        "ylabel": ylabel,
        "yscale": yscale,
        "host": host.info(),
        "data": [],
    }


def blkhash(
    input_type,
    digest_name=DIGEST,
    timeout_seconds=TIMEOUT,
    input_size=None,
    aio=None,
    queue_depth=None,
    threads=4,
    streams=STREAMS,
    cool_down=COOL_DOWN,
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

    time.sleep(cool_down)
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    print(description(r))
    return r


def openssl(
    digest_name=DIGEST,
    timeout_seconds=TIMEOUT,
    input_size=None,
    threads=1,
    cool_down=COOL_DOWN,
):
    cmd = [
        OPENSSL_BENCH,
        f"--digest-name={digest_name}",
        f"--timeout-seconds={timeout_seconds}",
        f"--threads={threads}",
    ]

    if input_size:
        cmd.append(f"--input-size={input_size}")

    time.sleep(cool_down)
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    print(description(r))
    return r


def description(r):
    hsize = format_humansize(r["total-size"])
    hrate = format_humansize(r["throughput"])
    return f"{r['threads']:>4} threads: {hsize} in {r['elapsed']:.3f} s ({hrate}/s)"


def format_humansize(n):
    for unit in ("bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"):
        if n < KiB:
            break
        n /= KiB
    return "{:.{precision}f} {}".format(n, unit, precision=0 if unit == "bytes" else 2)


def write(results, filename):
    with open(filename, "w") as f:
        json.dump(results, f, indent=2)
