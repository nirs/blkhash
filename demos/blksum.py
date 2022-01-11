# SPDX-FileCopyrightText: Red Hat Inc.
# SPDX-License-Identifier: LGPL-2.1-or-later

"""
blksum demo.

Installing requirements:

    python3 -m venv venv
    source venv/bin/activate
    pip install --upgrade pip nohands

Creating the images:

    virt-builder fedora-32 \
        --output fedora-32.qcow2 \
        --format=qcow2 \
        --hostname=fedora32 \
        --ssh-inject=root \
        --root-password=password:root \
        --selinux-relabel \
        --install=qemu-guest-agent

    qemu-img convert -f qcow2 -O raw fedora-32.qcow2 fedora-32.raw

    virt-builder fedora-34 \
        --output fedora-34.qcow2 \
        --format=qcow2 \
        --size=50G \
        --hostname=fedora34 \
        --ssh-inject=root \
        --root-password=password:root \
        --selinux-relabel \
        --install=qemu-guest-agent

    Start a VM using the fedora 34 image, and create 24 GiB of data. One
    way is to download linux kernel source and build a kernel. Duplicate
    the built kernel tree until you reach the wanted utilization.

    qemu-img create -f raw empty-8t.raw 8t

Run using:

    python3 blksum.py

"""

from nohands import *

run("clear")
msg("### Compute disk image checksum with blksum ###", color=YELLOW)
msg()
msg("These images are indentical:")
run("ls", "-lhs", "fedora-32.raw", "fedora-32.qcow2")
run("qemu-img", "compare", "-p", "fedora-32.raw", "fedora-32.qcow2")
msg()
msg("But sha1sum computes different checksums:")
run("sha1sum", "fedora-32.raw", "fedora-32.qcow2")
msg()
msg("blksum understands image formats!", color=YELLOW)
msg()
run("blksum", "sha1", "-p", "fedora-32.raw")
run("blksum", "sha1", "-p", "fedora-32.qcow2")
msg()
msg("blksum is fast!", color=YELLOW)
msg()
msg("50 GiB image with 24 GiB of data:")
run("ls", "-lhs", "fedora-34.qcow2")
run("blksum", "sha1", "-p", "fedora-34.qcow2")
msg()
msg("Empty 8 TiB image:")
run("ls", "-lhs", "empty-8t.raw")
run("blksum", "sha256", "-p", "empty-8t.raw")
msg()
msg("Created with https://github.com/nirs/demo", color=GREY)
