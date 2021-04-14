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
DIGEST_NAMES = ["sha1", "blake2b512"]
BLKSUM = os.environ.get("BLKSUM", "build/blksum")


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

    checksum = simple_blksum(md, image_raw)
    print(checksum)

    # Test raw format.
    assert blksum_file(md, image_raw) == [checksum, image_raw]
    assert blksum_pipe(md, image_raw) == [checksum, "-"]
    with open_nbd(image_raw, "raw") as nbd_url:
        assert blksum_nbd(md, nbd_url) == [checksum, nbd_url]

    # Test qcow2 format.
    image_qcow2 = str(tmpdir.join("image.qcow2"))
    convert_image(image_raw, image_qcow2, "qcow2")
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
    h = hashlib.new(md)

    with open(image, "rb") as f:
        while True:
            block = f.read(BLOCK_SIZE)
            if not block:
                break
            digest = hashlib.new(md, block).digest()
            h.update(digest)

    return h.hexdigest()


@contextmanager
def open_nbd(image, format):
    sock = image + ".sock"
    pidfile = image + ".pid"
    p = subprocess.Popen([
        "qemu-nbd",
        "--read-only",
        "--persistent",
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
