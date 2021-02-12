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
import subprocess
import pytest

BLOCK_SIZE = 1024**2
DIGEST_NAMES = ["sha1", "blake2b512"]


@pytest.fixture
def image(tmpdir):
    return str(tmpdir.join("image"))


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_sparse(md, image):
    create_image(image, "-- -- --")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_sparse_unaligned(md, image):
    create_image(image, "-- -- -")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_zero(md, image):
    create_image(image, "00 00 00")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_zero_unaligned(md, image):
    create_image(image, "00 00 0")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_full(md, image):
    create_image(image, "AB CD EF")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_full_unaligned(md, image):
    create_image(image, "AB CD E")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_mix(md, image):
    create_image(image, "A- -0 E- -0")
    assert blksum(md, image) == simple_blksum(md, image)


@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_mix_unaligned(md, image):
    create_image(image, "A- -0 E- -")
    assert blksum(md, image) == simple_blksum(md, image)


def blksum(md, image):
    out = subprocess.check_output(["./blksum", md, image])
    return out.decode()


def simple_blksum(md, image):
    h = hashlib.new(md)

    with open(image, "rb") as f:
        while True:
            block = f.read(BLOCK_SIZE)
            if not block:
                break
            digest = hashlib.new(md, block).digest()
            h.update(digest)

    return "{}  {}\n".format(h.hexdigest(), image)


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
