# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import argparse
import copy
import json
import os
import subprocess
import time

import host
import perf
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

THREADS = 64
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
        default=THREADS,
        help=f"Maximum number of cpus to test (default {THREADS})",
    )
    p.add_argument(
        "--host-name",
        help="Host name for graphs",
    )
    p.add_argument(
        "--image-dir",
        default="/data/tmp/blksum",
        help="Images directory",
    )
    p.add_argument(
        "-o",
        "--out-dir",
        default=".",
        help="Output directory",
    )
    return p.parse_args()


def threads(limit=64):
    # Powers of 2 with extra samples between. 12 samples instead of 64 saves
    # lot of testing time.
    samples = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64}
    online_cpus = os.sysconf("SC_NPROCESSORS_ONLN")
    limit = min(limit, online_cpus)
    samples.add(limit)
    return [v for v in sorted(samples) if v <= limit]


def results(
    test_name,
    host_name=None,
    yscale="linear",
):
    return {
        "test-name": test_name,
        "host-name": host_name,
        "xlabel": "threads",
        "xscale": "linear",
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
    cool_down=COOL_DOWN,
):
    cmd = [
        BLKHASH_BENCH,
        f"--input-type={input_type}",
        f"--digest-name={digest_name}",
        f"--timeout-seconds={timeout_seconds}",
        f"--threads={threads}",
        f"--read-size={read_size}",
        f"--block-size={block_size}",
    ]

    if input_size:
        cmd.append(f"--input-size={input_size}")
    if aio:
        cmd.append("--aio")
        if queue_depth is None:
            # Queue depth is important for I/O so we won't want go use less than
            # 16. When using many threads we want to match the number of
            # threads to ensures that threads has enough work in the queue.
            queue_depth = max(16, threads)
        cmd.append(f"--queue-depth={queue_depth}")

    time.sleep(cool_down)
    return _run_with_stats(cmd)


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
    return _run_with_stats(cmd)


def _run_with_stats(cmd):
    if perf.is_available():
        stats, stdout = perf.stat(cmd, events=("cycles:u",), capture_stdout=True)
        r = json.loads(stdout)
        if "cycles:u" in stats:
            r["cycles"] = stats["cycles:u"]["counter-value"]
            r["cpb"] = r["cycles"] / r["total-size"]
    else:
        cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
        r = json.loads(cp.stdout)
    print(description(r))
    return r


def blksum(
    filename,
    output=None,
    digest_name=DIGEST,
    max_threads=None,
    queue_depth=None,
    read_size=READ_SIZE,
    block_size=BLOCK_SIZE,
    cache=False,
    image_cached=False,
    pipe=False,
    runs=RUNS,
    label=None,
    cool_down=None,
):
    command = [
        "build/bin/blksum",
        f"--digest={digest_name}",
        "--threads={t}",  # Expanded by hyperfile.
        f"--read-size={read_size}",
        f"--block-size={block_size}",
    ]
    if queue_depth:
        command.append(f"--queue-depth={queue_depth}")
    if cache:
        command.append("--cache")

    if pipe:
        command.append("<" + filename)
    else:
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
    try:
        subprocess.run(cmd, check=True)
    finally:
        if image_cached:
            uncache_image(filename)

    amend_output(filename, output, label=label)


def mmap(
    filename,
    output=None,
    digest_name=DIGEST,
    max_threads=None,
    queue_depth=None,
    read_size=READ_SIZE,
    block_size=BLOCK_SIZE,
    runs=RUNS,
    label=None,
    cool_down=None,
):
    """
    mmap-bench [-d DIGEST|--digest-name=DIGEST]
           [-a|--aio] [-q N|--queue-depth N]
           [-t N|--threads N] [-b N|--block-size N]
           [-r N|--read-size N] [-h|--help]
           filename
    """
    command = [
        "build/test/mmap-bench",
        f"--digest-name={digest_name}",
        "--aio",
        "--threads={t}",  # Expanded by hyperfile.
        f"--read-size={read_size}",
        f"--block-size={block_size}",
    ]
    if queue_depth:
        command.append(f"--queue-depth={queue_depth}")

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

    cache_image(filename)
    try:
        subprocess.run(cmd, check=True)
    finally:
        uncache_image(filename)

    amend_output(filename, output, label=label)


def openssl(
    filename,
    digest_name=DIGEST,
    output=None,
    max_threads=None,
    pipe=False,
    runs=RUNS,
    label=None,
    cool_down=None,
):
    command = ["openssl", digest_name]

    if pipe:
        command.append("<" + filename)
    else:
        command.append(filename)

    cmd = [
        "hyperfine",
        f"--runs={runs}",
        "--time-unit=second",
        "--parameter-list",
        "t",
        "1",
    ]
    if cool_down:
        cmd.append(f"--prepare=sleep {cool_down}")
    if output:
        cmd.append(f"--export-json={output}")
    cmd.append(" ".join(command))

    cache_image(filename)
    try:
        subprocess.run(cmd, check=True)
    finally:
        uncache_image(filename)

    amend_output(filename, output, label=label, gen_max_threads=max_threads)


def b3sum(
    filename,
    output=None,
    max_threads=None,
    pipe=False,
    runs=RUNS,
    label=None,
    cool_down=None,
):
    command = ["b3sum", "--num-threads={t}"]

    if pipe:
        command.append("<" + filename)
        threads_params = "1"
    else:
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

    cache_image(filename)
    try:
        subprocess.run(cmd, check=True)
    finally:
        uncache_image(filename)

    amend_output(filename, output, label=label, gen_max_threads=max_threads)


def uncache_image(filename):
    cmd = ["build/test/cache", "--drop", filename]
    subprocess.run(cmd, check=True)


def cache_image(filename):
    """
    Ensure image is cached. Thorectically reading the image once is enough, but
    in practice the we need to read it twice, and in some cases reading 3 times
    gives more consistent results.
    """
    cmd = ["build/test/cache", filename]

    subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    stats = cp.stdout.decode().strip()

    print("image:")
    print(f"  filename: {filename}")
    print(f"  stats: {stats}")
    print()


def amend_output(filename, output, label=None, gen_max_threads=None):
    with open(output) as f:
        data = json.load(f)

    info = image_info(filename)
    data["size"] = info["virtual-size"]

    if (
        gen_max_threads is not None
        and len(data["results"]) == 1
        and data["results"][0]["parameters"]["t"] == "1"
    ):
        # Generate result with max_threads.
        results = data["results"]
        generated = copy.deepcopy(results[0])
        generated["parameters"]["t"] = str(gen_max_threads)
        results.append(generated)

    if label:
        data["label"] = label

    with open(output, "w") as f:
        json.dump(data, f)


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
    hrate = format_humansize(r["throughput"])
    if "cpb" in r:
        # For holes we have very low cpb value (e.g. 0.0003).
        return f"{r['threads']:>4} threads: {hrate}/s, {r['cpb']:.4f} cpb"
    else:
        return f"{r['threads']:>4} threads: {hrate}/s"


def format_humansize(n):
    for unit in ("bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"):
        if n < KiB:
            break
        n /= KiB
    return "{:.{precision}f} {}".format(n, unit, precision=0 if unit == "bytes" else 2)


def write(results, filename):
    outdir = os.path.dirname(filename)
    os.makedirs(outdir, exist_ok=True)
    with open(filename, "w") as f:
        json.dump(results, f, indent=2)
