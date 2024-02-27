# Running blkhash benchmarks

This page describes how to reproduce blkhash and blksum benchmark results.

## Installing required packages and tools

### rust tools

Install b3sum and hyperfine:
- b3sum: Used as a reference for the blksum using blake3 digest.
- hyperfine: Use to run blksum and b3sum benchmarks

```
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env
cargo install b3sum hyperfine
```

### virt-* tools

Used to create test images for blksum tests. This uses virt-builder
which requires nested virtualization support.

```
sudo dnf install guestfs-tools
```

If you cannot run virt-builder on the benchmark machine, you can create
the test images on another machine and copy the images to the benchmark
machine.

You can compress and copy the qcow2 images to the benchmark host:

```
xz --threads 0 --keep *.qcow2
scp *.xz user@bench.host.example.com:blksum/
```

On the host, decompress the qcow2 images and recreate the raw images:

```
xz --threads 0 --decompress *.xz
for n in 20p 40p 80p; do
    qemu-img convert -f qcow2 -O raw $n.qcow2 $n.raw
done
```

### matplotlib

Used to create graphs from benchmark results.

```
sudo apt install python3-venv
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip matplotlib
```

## Running the benchmarks

### Disable SMT (HyperThreading)

HyperThreading on Intel CPUs is typically not very useful for this
workload, and gives confusing results.

```
sudo sh -c "echo off > /sys/devices/system/cpu/smt/control"
```

### blkhash benchmarks

The benchmarks are mostly automated, but you may need to modify some
arguments:

- `--timeout`: the default is 2 seconds, but in some environment you may
  need longer runs to get more consistent results.

- `--cool-down`: the default is 6 seconds, good for laptop that tend to
  heat and slow down, giving inconsistent results. In some environment
  you get more consistent results with zero cool down time.

- `--digest-name`: If not set the benchmark use `sha256`. If blake3 is
  available on the host, you can use `--digest-name blake3`.

- `--output`: without output option, the benchmarks do not save the
  results json file. You can use any filename.

To create graphs from the benchmarks run:

```
test/plot.py bechmark-output.json
```

This creates the png file:

```
benchmark-output.png
```

Example run with blake3 digest:

```
test/bench-blkhash-digest.py \
    --output blkhash-digest.json

test/plot.py blkhash-digest.json

test/bench-blkhash-parallel.py \
    --digest-name blake3 \
    --output blkhash-parallel-blake3.json

test/plot.py blkhash-parallel-blake3.json

test/bench-blkhash-zero-optimization.py \
    --digest-name blake3 \
    --output blkhash-zero-optimization-blake3.json

test/plot.py blkhash-zero-optimization-blake3.json
```

### blksum benchmarks

These benchmarks are automated. The benchmarks create one or more json
output files, in the current directory, and finally a graph from all the
json files.

Important arguments:

- `--cool-down`: not set by default. If needed on your environment to
  get consistent results you need to set this manually when running the
  benchmark. Typically cool-down time of 3 times of the runtime with
  largest number of threads gives most consistent results on laptops.

Example run with blake3 digest:

```
test/bench-b3sum.py

test/bench-blksum-blake3-nbd.py
```
