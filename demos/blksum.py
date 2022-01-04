"""
Requires https://github.com/nirs/demo

Run using:

    export PYTHONPATH=$HOME/src/demo
    python3 blksum.py

"""

from demo import *

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
