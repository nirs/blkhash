# blkhash - block based checksum for disk images
# Copyright Nir Soffer <nirsof@gmail.com>.
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301 USA

import hashlib
import os
import subprocess
import time

from contextlib import contextmanager

import pytest

BLOCK_SIZE = 64 * 1024
SEGMENT_SIZE = 128 * 1024 * 1024
DIGEST_NAMES = ["sha1", "blake2b512"]
BLKSUM = os.environ.get("BLKSUM", "build/blksum")
HAVE_NBD = bool(os.environ.get("HAVE_NBD"))


@pytest.mark.parametrize("fmt", [
    pytest.param("AA", id="block-data"),
    pytest.param("00", id="block-zero"),
    pytest.param("--", id="block-sparse"),
    pytest.param("A", id="partial-block-data"),
    pytest.param("0", id="partial-block-zero"),
    pytest.param("-", id="partial-block-sparse"),
    pytest.param("-- -- --", id="sparse"),
    pytest.param("-- -- -", id="sparse-unaligned"),
    pytest.param("00 00 00", id="zero"),
    pytest.param("00 00 0", id="zero-unaligned"),
    pytest.param("AB CD EF", id="full"),
    pytest.param("AB CD E", id="full-unaligned"),
    pytest.param("A- -0 E- -0", id="mix"),
    pytest.param("A- -0 E- -", id="mix-unaligned"),
])
@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_blksum(tmpdir, fmt, md):
    image_raw = str(tmpdir.join("image.raw"))
    create_image(image_raw, fmt)
    image_qcow2 = str(tmpdir.join("image.qcow2"))
    convert_image(image_raw, image_qcow2, "qcow2")

    checksum = simple_blksum(md, image_raw)
    print(checksum)

    # Test file.
    assert blksum_file(md, image_raw) == [checksum, image_raw]
    if HAVE_NBD:
        # We can test also qcow2 format.
        assert blksum_file(md, image_qcow2) == [checksum, image_qcow2]

    # Test pipe- blksum cannot process qcow2 via pipe.
    assert blksum_pipe(md, image_raw) == [checksum, "-"]

    if HAVE_NBD:
        # Test using external qemu-nbd process.
        with open_nbd(image_raw, "raw") as nbd_url:
            assert blksum_nbd(md, nbd_url) == [checksum, nbd_url]
        with open_nbd(image_qcow2, "qcow2") as nbd_url:
            assert blksum_nbd(md, nbd_url) == [checksum, nbd_url]


def blksum_nbd(md, nbd_url):
    out = subprocess.check_output([BLKSUM, md, nbd_url])
    return out.decode().strip().split("  ")


def blksum_file(md, image):
    out = subprocess.check_output([BLKSUM, md, image])
    return out.decode().strip().split("  ")


def blksum_pipe(md, image):
    with open(image) as f:
        r = subprocess.run(
            [BLKSUM, md],
            stdin=f,
            stdout=subprocess.PIPE,
            check=True,
        )
    return r.stdout.decode().strip().split("  ")


def simple_blksum(md, image):
    """
    Compute root hash for segments:

        H( H(segment 1) + H(segment 2) ... H(segment N)) )

    H(segment N) is:

        H( H(block 1) + H(block 2) ... H(block M)) )
    """
    size = os.path.getsize(image)
    root = hashlib.new(md)

    with open(image, "rb") as f:
        for offset in range(0, size, SEGMENT_SIZE):
            length = min(SEGMENT_SIZE, size - offset)

            # Compute segment digest.
            segment = hashlib.new(md)

            while length:
                # Compute block digest.
                n = min(BLOCK_SIZE, length)
                block = f.read(n)
                block_digest = hashlib.new(md, block).digest()
                segment.update(block_digest)
                length -= n

            root.update(segment.digest())

    return root.hexdigest()


@contextmanager
def open_nbd(image, format):
    sock = image + ".sock"
    pidfile = image + ".pid"
    p = subprocess.Popen([
        "qemu-nbd",
        "--read-only",
        "--persistent",
        "--shared=4",
        "--socket={}".format(sock),
        "--pid-file={}".format(pidfile),
        "--format={}".format(format),
        image,
    ])
    try:
        while not os.path.exists(pidfile):
            time.sleep(0.005)
            if p.poll() is not None:
                raise RuntimeError("Error running qemu-nbd")

        yield "nbd+unix:///?socket=" + sock
    finally:
        p.kill()
        p.wait()


def convert_image(src, dst, format):
    subprocess.check_call(
        ["qemu-img", "convert", "-f", "raw", "-O", format, src, dst])


def create_image(path, fmt, block_size=BLOCK_SIZE // 2):
    """
    Create image specified by foramt string.

    Special characters:
        "-" creates a hole.
        "0" create zero block
        " " ignored for more readable format

    Any other character creates a block with that chracter.
    """
    fmt = fmt.replace(" ", "")

    with open(path, "wb") as f:
        f.truncate(len(fmt) * block_size)

        for i, c in enumerate(fmt):
            if c == "-":
                continue

            if c == "0":
                c = "\0"

            f.seek(i * block_size)
            f.write(c.encode("ascii") * block_size)
