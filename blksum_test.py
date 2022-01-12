# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import hashlib
import os
import subprocess
import time

from contextlib import contextmanager

import pytest
import blksum

DIGEST_NAMES = ["sha1", "blake2b512"]
BLKSUM = os.environ.get("BLKSUM", "build/blksum")
HAVE_NBD = bool(os.environ.get("HAVE_NBD"))


@pytest.mark.parametrize("fmt", [
    pytest.param(
        "64k:A",
        id="block-data"),
    pytest.param(
        "64K:0",
        id="block-zero"),
    pytest.param(
        "64k:-",
        id="block-sparse"),
    pytest.param(
        "32k:A",
        id="partial-block-data"),
    pytest.param(
        "32k:0",
        id="partial-block-zero"),
    pytest.param(
        "32k:-",
        id="partial-block-sparse"),
    pytest.param(
        "192k:-",
        id="sparse"),
    pytest.param(
        "160k:-",
        id="sparse-unaligned"),
    pytest.param(
        "192k:0",
        id="zero"),
    pytest.param(
        "160k:0",
        id="zero-unaligned"),
    pytest.param(
        "32k:A 32k:B 32k:C 32k:D 32k:E 32k:F",
        id="full"),
    pytest.param(
        "32k:A 32k:B 32k:C 32k:D 32k:E",
        id="full-unaligned"),
    pytest.param(
        "32k:A 64k:- 32k:0 32k:E 64k:- 32k:0",
        id="mix"),
    pytest.param(
        "32k:A 64k:- 32k:0 32k:E 64k:-",
        id="mix-unaligned"),
    pytest.param(
        "256k:A 64k:B 64k:- 320k:C 64k:- 64k:D",
        id="read-size"),
    pytest.param(
        "1m:A 127m:-",
        id="1-segment"),
    pytest.param(
        "1m:A 127m:-  1m:B 127m:-  1m:C 63m:-",
        id="2.5-segments"),
    pytest.param(
        "1m:A 127m:-  1m:B 127m:-  1m:C 127m:-  1m:D 127m:-",
        id="4-segments"),
    pytest.param(
        "1m:A 127m:-  1m:B 127m:-  1m:C 127m:-  1m:D 127m:-  1m:E 127m:-",
        id="5-segments"),
])
@pytest.mark.parametrize("md", DIGEST_NAMES)
def test_blksum(tmpdir, fmt, md):
    image_raw = str(tmpdir.join("image.raw"))
    create_image(image_raw, fmt)
    image_qcow2 = str(tmpdir.join("image.qcow2"))
    convert_image(image_raw, image_qcow2, "qcow2")

    checksum = blksum.checksum(md, image_raw)
    print(checksum)

    # Test file.
    res = [checksum, image_raw]
    assert blksum_file(md, image_raw, cache=True) == res
    assert blksum_file(md, image_raw, cache=False) == res

    if HAVE_NBD:
        # We can test also qcow2 format.
        res = [checksum, image_qcow2]
        assert blksum_file(md, image_qcow2, cache=True) == res
        assert blksum_file(md, image_qcow2, cache=False) == res

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


def blksum_file(md, image, cache=True):
    cmd = [BLKSUM, md]
    if cache:
        cmd.append("--cache")
    cmd.append(image)
    out = subprocess.check_output(cmd)
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


def create_image(path, fmt):
    """
    Create image specified by format string.

    Special characters:
        " " extent separator
        ":" size:byte separator
        "-" hole byte
        "0" zero byte

    Any other character creates an extent with that chracter.

    Example:

        "4096:A  64k:B  129m:-  512:C  512:0"

        Create an image with 5 extents:

        extent  size        content    type
        -----------------------------------
        #1      4096        "A"        data
        #2      64 KiB      "B"        data
        #3      129 MiB     "\0"       hole
        #4      512         "C"        data
        #5      512         "\0"       data

    """
    write_size = 1024**2

    with open(path, "wb") as f:
        extents = [x.split(":", 1) for x in fmt.split(None)]
        for hsize, c in extents:
            size = humansize(hsize)

            if c == "-":
                # Create a hole.
                f.truncate(f.tell() + size)
                f.seek(0, os.SEEK_END)

            else:
                # Write data.
                if c == "0":
                    c = "\0"

                b = c.encode("ascii")

                while size > write_size:
                    f.write(b * write_size)
                    size -= write_size

                f.write(b * size)


def humansize(s):
    if s.isdigit():
        return int(s)
    value, unit = s[:-1], s[-1]
    value = int(value)
    unit = unit.lower()
    if unit == "k":
        return value * 1024
    elif unit == "m":
        return value * 1024**2
    elif unit == "g":
        return value * 1024**3
    else:
        raise ValueError("Unsupported unit: %r" % unit)
