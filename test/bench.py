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
DIGEST_BENCH = os.path.join(build_dir, "digest-bench")

DIGEST = "sha256"
RUNS = 10

# Optimal value reading from qemu-nbd. Using higher values typically do not
# improve read throughput, but is required when using larger number of threads.
QUEUE_DEPTH = 16

STREAMS = 64
READ_SIZE = "256k"
BLOCK_SIZE = "64k"
TIMEOUT = 2
COOL_DOWN = 6


def parse_args():
    p = argparse.ArgumentParser("bench")
    p.add_argument(
        "--digest-name",
        default=DIGEST,
        help=f"Digest name (default {DIGEST})",
    )
    p.add_argument(
        "--queue-depth",
        help=f"Number of inflight requests (default number of threads, minimum "
             f"{QUEUE_DEPTH})",
    )
    p.add_argument(
        "--read-size",
        default=READ_SIZE,
        help=f"Size of read buffer (default {READ_SIZE})",
    )
    p.add_argument(
        "--block-size",
        default=BLOCK_SIZE,
        help=f"Hash block size (default {BLOCK_SIZE})",
    )
    p.add_argument(
        "--runs",
        default=RUNS,
        type=int,
        help=f"Number of runs for blksum and b3sum (default {RUNS})",
    )
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
        "--max-threads",
        type=int,
        default=STREAMS,
        help=f"Maximum number of cpus to test (default {STREAMS})",
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


def threads(limit=64):
    online_cpus = os.sysconf("SC_NPROCESSORS_ONLN")
    return range(1, min(online_cpus, limit) + 1)


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


def build(nbd=None, blake3=None):
    cmd = ["meson", "configure", "build"]
    if nbd:
        cmd.append(f"-Dnbd={nbd}")
    if blake3:
        cmd.append(f"-Dblake3={blake3}")
    subprocess.run(cmd, check=True)

    cmd = ["meson", "compile", "-C", "build"]
    subprocess.run(cmd, check=True)


def blkhash(
    input_type,
    digest_name=DIGEST,
    timeout_seconds=TIMEOUT,
    input_size=None,
    aio=None,
    queue_depth=None,
    read_size=READ_SIZE,
    block_size=BLOCK_SIZE,
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
        f"--read-size={read_size}",
        f"--block-size={block_size}",
    ]

    if input_size:
        cmd.append(f"--input-size={input_size}")
    if aio:
        cmd.append("--aio")
        if queue_depth is None:
            # Queue depth is imporant for I/O so we won't want go use less than
            # 16. When using many threads we want to match the number of
            # threads to ensures that threads has enough work in the queue.
            queue_depth = max(16, threads)
        cmd.append(f"--queue-depth={queue_depth}")


    time.sleep(cool_down)
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    r = json.loads(cp.stdout)
    print(description(r))
    return r


def digest(
    digest_name=DIGEST,
    timeout_seconds=TIMEOUT,
    input_size=None,
    threads=1,
    cool_down=COOL_DOWN,
):
    cmd = [
        DIGEST_BENCH,
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


def blksum(
    filename,
    output=None,
    digest_name=DIGEST,
    max_threads=None,
    streams=STREAMS,
    queue_depth=None,
    read_size=READ_SIZE,
    block_size=BLOCK_SIZE,
    cache=False,
    image_cached=False,
    runs=RUNS,
    cool_down=None,
):
    command = [
        "build/bin/blksum",
        f"--digest={digest_name}",
        "--threads={t}",
        f"--streams={streams}",
        f"--read-size={read_size}",
        f"--block-size={block_size}",
    ]
    if queue_depth:
        command.append(f"--queue-depth={queue_depth}")
    if cache:
        command.append("--cache")
    command.append(filename)

    threads_params = ",".join(str(n) for n in threads(max_threads))
    cmd = [
        "hyperfine",
        f"--runs={runs}",
        "--time-unit=second",
        "--parameter-list",
        "t",
        threads_params,
    ]
    if cool_down:
        cmd.append(f"--prepare=sleep {cool_down}")
    if output:
        cmd.append(f"--export-json={output}")
    cmd.append(" ".join(command))

    if image_cached:
        cache_image(filename)

    subprocess.run(cmd, check=True)
    add_image_info(filename, output)


def b3sum(
    filename,
    output=None,
    max_threads=None,
    runs=RUNS,
    cool_down=None,
):
    command = ["b3sum", "--num-threads={t}", filename]
    threads_params = ",".join(str(n) for n in threads(max_threads))
    cmd = [
        "hyperfine",
        f"--runs={runs}",
        "--time-unit=second",
        "--parameter-list",
        "t",
        threads_params,
    ]
    if cool_down:
        cmd.append(f"--prepare=sleep {cool_down}")
    if output:
        cmd.append(f"--export-json={output}")
    cmd.append(" ".join(command))

    cache_image(filename)
    subprocess.run(cmd, check=True)
    add_image_info(filename, output)


def cache_image(filename):
    """
    Ensure image is cached. Thorectically reading the image once is enough, but
    in practice the we need to read it twice, and in some cases reading 3 times
    gives more consitent results.
    """
    cmd = ["dd", f"if={filename}", "bs=1M", "of=/dev/null"]
    for i in range(3):
        cp = subprocess.run(cmd, check=True, stderr=subprocess.PIPE)
        stats = cp.stderr.decode().splitlines()[-1]

    print("image:")
    print(f"  filename: {filename}")
    print(f"  stats: {stats}")
    print()


def add_image_info(filename, output):
    info = image_info(filename)
    with open(output) as f:
        results = json.load(f)
    results["size"] = info["virtual-size"]
    with open(output, "w") as f:
        json.dump(results, f)


def plot_blksum(*files, title=None, output=None):
    cmd = ["test/plot-blksum.py"]
    if title:
        cmd.append(f"--title={title}")
    if output:
        cmd.append(f"--output={output}")
    cmd.extend(files)
    subprocess.run(cmd, check=True)


def image_info(filename):
    cmd = ["qemu-img", "info", "--output=json", filename]
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    return json.loads(cp.stdout)


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
