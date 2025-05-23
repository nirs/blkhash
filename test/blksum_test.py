# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import glob
import hashlib
import os
import signal
import subprocess
import time

from collections import namedtuple
from contextlib import contextmanager

import pytest
import blkhash

DIGEST_NAMES = ["sha1", "blake2b512"]
BLKSUM = os.environ.get("BLKSUM", "build/bin/blksum")
HAVE_NBD = bool(os.environ.get("HAVE_NBD"))

Image = namedtuple("Image", "filename,md,checksum")
Child = namedtuple("Child", "pid,name")
NBDServer = namedtuple("NBDServer", "pid,url")

QCOW2_CLUSTER_SIZE = 64 * 1024

requires_nbd = pytest.mark.skipif(not HAVE_NBD, reason="NBD required")


@pytest.fixture(scope="session", params=[
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
def image(tmpdir_factory, request):
    filename = str(tmpdir_factory.mktemp("image").join("raw"))
    print(f"Creating raw image {filename}")
    create_image(filename, request.param)
    return filename


@pytest.fixture(scope="session", params=DIGEST_NAMES)
def raw(image, request):
    print(f"Computing {request.param} checksum for {image}")
    checksum = blkhash.checksum(image, request.param)
    return Image(image, request.param, checksum)


@pytest.fixture(scope="session")
def qcow2(raw):
    filename = raw.filename.replace("raw", "qcow2")
    print(f"Creating qcow2 image {filename}")
    convert_image(raw.filename, filename, "qcow2")
    return Image(filename, raw.md, raw.checksum)


@pytest.fixture(scope="session")
def extents_raw(tmpdir_factory):
    filename = str(tmpdir_factory.mktemp("extents").join("raw"))
    print(f"Creating image {filename}")
    # Create image with 8192 extents for testing extents handling.  We write
    # one byte in every 2 qcow2 clusters, creating image with one data cluster
    # and one unallocated cluster pattern.
    with open(filename, "wb") as f:
        f.truncate(4096 * QCOW2_CLUSTER_SIZE * 2)
        for i in range(4096):
            f.seek(i * QCOW2_CLUSTER_SIZE * 2, os.SEEK_SET)
            f.write(b"U")
    checksum = blkhash.checksum(filename, "sha256")
    return Image(filename, "sha256", checksum)


@pytest.fixture(scope="session")
def extents_qcow2(extents_raw):
    filename = extents_raw.filename.replace("raw", "qcow2")
    print(f"Creating qcow2 image {filename}")
    convert_image(extents_raw.filename, filename, "qcow2")
    return Image(filename, extents_raw.md, extents_raw.checksum)


@pytest.fixture(scope="session")
def term(tmpdir_factory):
    filename = str(tmpdir_factory.mktemp("image").join("term"))
    print(f"Creating image {filename}")
    # Create image with 50,000 extents. This should be slow enough for testing
    # termination behavior.
    block = bytearray(4096)
    with open(filename, "wb") as f:
        for i in range(25000):
            f.seek(124 * 1024, os.SEEK_CUR)
            f.write(block)
    return filename


@pytest.mark.parametrize("cache", [True, False])
def test_raw_file(raw, cache):
    res = blksum_file(raw.filename, md=raw.md, cache=cache)
    assert res == [raw.checksum, raw.filename]


@pytest.mark.parametrize("cache", [True, False])
@requires_nbd
def test_qcow2_file(qcow2, cache):
    res = blksum_file(qcow2.filename, md=qcow2.md, cache=cache)
    assert res == [qcow2.checksum, qcow2.filename]


def test_raw_pipe(raw, cache):
    res = blksum_pipe(raw.filename, md=raw.md)
    assert res == [raw.checksum, "-"]


@requires_nbd
def test_raw_nbd(tmpdir, raw, cache):
    with open_nbd(tmpdir, raw.filename, "raw") as nbd:
        res = blksum_nbd(nbd.url, md=raw.md)
    assert res == [raw.checksum, nbd.url]


@requires_nbd
def test_qcow2_nbd(tmpdir, qcow2, cache):
    with open_nbd(tmpdir, qcow2.filename, "qcow2") as nbd:
        res = blksum_nbd(nbd.url, md=qcow2.md)
    assert res == [qcow2.checksum, nbd.url]


@requires_nbd
def test_extents_raw(tmpdir, extents_raw):
    res = blksum_file(extents_raw.filename, md=extents_raw.md)
    assert res == [extents_raw.checksum, extents_raw.filename]


@requires_nbd
def test_extents_qcow2(tmpdir, extents_qcow2):
    res = blksum_file(extents_qcow2.filename, md=extents_qcow2.md)
    assert res == [extents_qcow2.checksum, extents_qcow2.filename]


def test_list_digests():
    out = subprocess.check_output([BLKSUM, "--list-digests"])
    blksum_digests = out.decode().strip().splitlines()

    # Extracting the list of provided digests from openssl is hard, so lets
    # check that we got some results.
    assert blksum_digests

    # And that every digest name can be used in python to create a new hash.
    # This is important for compatibility with python hashlib module.
    # The "null" digest is not available in Centos 8 using openssl 1.1, so we
    # skip it.
    for name in blksum_digests:
        if name in ("null", "blake3"):
            continue
        hashlib.new(name)


def test_default_digest(tmpdir):
    path = tmpdir.join("empty.raw")
    create_image(path, "1m:-")
    assert blksum_file(path) == blksum_file(path, md="sha256")


def test_null_digest(tmpdir):
    path = tmpdir.join("empty.raw")
    create_image(path, "1m:-")
    # The special "null" digest does nothing and return empty checksum.
    assert blksum_file(path, md="null") == ["", path]


signals_params = pytest.mark.parametrize("signo,error", [
    pytest.param(signal.SIGINT, "", id="sigint"),
    pytest.param(
        signal.SIGTERM, "blksum: Terminated by signal 15\n", id="sigterm"),
])


@signals_params
def test_term_signal_file(term, signo, error):
    remove_tempdirs()
    bs = Blksum(filename=term)

    if HAVE_NBD:
        sock = bs.wait_for_socket()
        qemu_nbd = bs.children()[0]

    time.sleep(0.2)
    bs.send_signal(signo)
    bs.wait()

    assert bs.returncode == -signo
    assert bs.out == ""
    assert bs.err == error

    if HAVE_NBD:
        assert not os.path.isdir(f"/proc/{qemu_nbd.pid}")
        assert not os.path.isdir(os.path.dirname(sock))


@requires_nbd
@signals_params
def test_term_signal_nbd(tmpdir, term, signo, error):
    with open_nbd(tmpdir, term, "raw") as nbd:
        bs = Blksum(filename=nbd.url)
        time.sleep(0.2)
        bs.send_signal(signo)
        bs.wait()

        assert bs.returncode == -signo
        assert bs.out == ""
        assert bs.err == error


@pytest.mark.xfail(reason="Fails randomly in gitlab")
@signals_params
def test_term_signal_pipe(term, signo, error):
    with open(term) as f:
        bs = Blksum(stdin=f)
        time.sleep(0.2)
        bs.send_signal(signo)
        bs.wait()

    assert bs.returncode == -signo
    assert bs.out == ""
    assert bs.err == error


@requires_nbd
def test_term_qemu_nbd_file(term):
    remove_tempdirs()
    bs = Blksum(filename=term)
    sock = bs.wait_for_socket()
    qemu_nbd = bs.children()[0]

    time.sleep(0.2)
    os.kill(qemu_nbd.pid, signal.SIGTERM)
    bs.wait()

    assert bs.returncode == 1
    assert bs.out == ""
    assert bs.err != ""
    assert not os.path.isdir(f"/proc/{qemu_nbd.pid}")
    assert not os.path.isdir(os.path.dirname(sock))


@requires_nbd
def test_term_qemu_nbd_url(tmpdir, term):
    with open_nbd(tmpdir, term, "raw") as nbd:
        bs = Blksum(filename=nbd.url)
        time.sleep(0.2)
        os.kill(nbd.pid, signal.SIGTERM)
        bs.wait()

        assert bs.returncode == 1
        assert bs.out == ""
        assert bs.err != ""


class Blksum:

    def __init__(self, filename=None, digest=None, cache=None, stdin=None,
                 timeout=10):
        self.filename = filename
        self.digest = digest
        self.cache = cache
        self.stdin = stdin

        self.cmd = [BLKSUM]
        if self.digest:
            self.cmd.append("--digest")
            self.cmd.append(self.digest)
        if self.cache:
            self.cmd.append("--cache")
        if self.filename:
            self.cmd.append(self.filename)

        self.proc = subprocess.Popen(
            self.cmd,
            stdin=self.stdin,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

        self.returncode = None
        self.out = None
        self.err = None

    def wait(self, check=False):
        out, err = self.proc.communicate()

        self.out = out.decode()
        self.err = err.decode()
        self.returncode = self.proc.returncode

        if check and self.returncode != 0:
            raise RuntimeError(
                f"Command {self.cmd} failed: returncode={self.returncode} "
                f"out={self.out} err={self.err}")

    def send_signal(self, signo):
        self.proc.send_signal(signo)

    def wait_for_socket(self, timeout=10):
        start = time.monotonic()
        deadline = start + timeout
        delay = 0.005

        while True:
            time.sleep(delay)
            delay = min(0.5, delay * 1.5)

            found = glob.glob("/tmp/blksum-*/sock")
            if found:
                wait = time.monotonic() - start
                print(f"Found {found} in {wait:.3f} seconds")
                return found[0]

            if time.monotonic() >= deadline:
                raise RuntimeError(
                    f"Timeout waiting for blksum")

    def children(self):
        cmd = ["pgrep", "--parent", str(self.proc.pid), "--list-name"]
        cp = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        if cp.returncode != 0:
            raise RuntimeError(
                f"Command {cmd} failed with rc={cp.returncode} "
                f"out={cp.stdout} err={cp.stderr}")

        children = []
        for line in cp.stdout.decode().splitlines():
            pid, name = line.split(None, 1)
            children.append(Child(pid=int(pid), name=name))

        print(f"Found child processes {children}")
        return children


def remove_tempdirs():
    """
    Remove leftovers from previous tests.
    """
    subprocess.run(["rm", "-rf", "/tmp/blksum-*"], check=True)


def blksum_nbd(nbd_url, md=None):
    bs = Blksum(filename=nbd_url, digest=md)
    bs.wait(check=True)
    return bs.out.rstrip().split("  ")


def blksum_file(image, md=None, cache=True):
    bs = Blksum(filename=image, digest=md, cache=cache)
    bs.wait(check=True)
    return bs.out.rstrip().split("  ")


def blksum_pipe(image, md=None):
    with open(image) as f:
        bs = Blksum(digest=md, stdin=f)
        bs.wait(check=True)
    return bs.out.rstrip().split("  ")


@contextmanager
def open_nbd(tmpdir, image, format):
    sockfile = str(tmpdir.join("sock"))
    pidfile = str(tmpdir.join("pid"))
    p = subprocess.Popen([
        "qemu-nbd",
        "--read-only",
        "--persistent",
        "--shared=4",
        "--socket={}".format(sockfile),
        "--pid-file={}".format(pidfile),
        "--format={}".format(format),
        image,
    ])
    try:
        while not os.path.exists(pidfile):
            time.sleep(0.005)
            if p.poll() is not None:
                raise RuntimeError("Error running qemu-nbd")

        yield NBDServer(
            pid=p.pid,
            url="nbd+unix:///?socket=" + sockfile)
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

    Any other character creates an extent with that character.

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
