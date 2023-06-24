#!/usr/bin/env python3
# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import argparse
import json
import os
import subprocess
import tempfile

from units import *

UTILIZAZTION = [0.1, 0.2, 0.4, 0.8]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--distro",
        default="fedora-38",
        help="Distro name (default 'fedora38')",
    )
    parser.add_argument(
        "--image-size",
        default="20G",
        help="Image virtual size (default '20G')",
    )
    parser.add_argument(
        "target_dir",
        help="Directory for creating images",
    )
    args = parser.parse_args()

    print(f"Creating target directory {args.target_dir}")
    os.makedirs(args.target_dir, exist_ok=True)

    base_image = os.path.join(args.target_dir, "base.raw")
    create_base_image(args.distro, args.image_size, base_image)

    base_info = image_info(base_image)
    print(f"Base image virtual-size: {base_info['virtual-size']}")
    print(f"Base image actual-size: {base_info['actual-size']}")

    for util in UTILIZAZTION:
        image_name = f"{int(util * 100)}p"

        current_size = base_info["actual-size"]
        wanted_size = int(util * base_info["virtual-size"])
        if current_size > wanted_size:
            print(f"Image current size too large, skipping image {image_name}")
            continue

        image_raw = os.path.join(args.target_dir, image_name + ".raw")
        convert_image(base_image, "raw", image_raw, "raw")

        with tempfile.TemporaryDirectory(dir="/var/tmp", prefix="blkhash-") as tmp_dir:
            create_data_files(tmp_dir, wanted_size - current_size)
            copy_data(image_raw, tmp_dir, "/var/tmp")

        image_qcow2 = os.path.join(args.target_dir, image_name + ".qcow2")
        convert_image(image_raw, "raw", image_qcow2, "qcow2")


def create_base_image(distro, size, path):
    if os.path.isfile(path):
        print(f"Reusing existing image {path}")
        return

    print(f"Creating base image {path} with size {size}")
    cmd = [
        "virt-builder",
        distro,
        f"--output={path}",
        "--format=raw",
        f"--size={size}",
        f"--hostname={distro}",
        "--ssh-inject=root",
        "--root-password=password:root",
        "--selinux-relabel",
        "--install=qemu-guest-agent",
    ]
    subprocess.run(cmd, check=True)


def image_info(path):
    cmd = ["qemu-img", "info", "--output=json", path]
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    return json.loads(cp.stdout)


def create_data_files(tmp_dir, size):
    data_dir = os.path.join(tmp_dir, "data")
    os.mkdir(data_dir)
    size_mb = size // MiB
    print(f"Creating {size_mb} MiB data files for image")
    for n in range(size_mb):
        chunk = "{n:032d}".encode()
        data = chunk * (MiB // len(chunk))
        path = os.path.join(data_dir, f"{n:04d}")
        with open(path, "wb") as f:
            f.write(data)


def convert_image(src, src_format, dst, dst_format):
    print(f"Creating image {dst}")
    cmd = ["qemu-img", "convert", "-f", src_format, "-O", dst_format, src, dst]
    subprocess.run(cmd, check=True)


def copy_data(image, src_dir, dst_dir):
    print(f"Copying data to image")
    cmd = ["virt-copy-in", "-a", image, src_dir, dst_dir]
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
