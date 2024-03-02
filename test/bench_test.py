# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import pytest
import bench
import blkhash
from units import MiB

DIGEST = "sha256"
INPUT_SIZE = "1m"
threads_params = pytest.mark.parametrize("threads", [1, 2, 4, 8, 16, 32, 64])


@threads_params
def test_blkhash_data_sha256(threads):
    r = bench.blkhash(
        input_type="data",
        input_size=INPUT_SIZE,
        digest_name=DIGEST,
        threads=threads,
        cool_down=0,
    )["checksum"]
    assert r == checksum(b"\x55" * MiB)


@threads_params
def test_blkhash_zero_sha256(threads):
    r = bench.blkhash(
        input_type="zero",
        input_size=INPUT_SIZE,
        digest_name=DIGEST,
        threads=threads,
        cool_down=0,
    )["checksum"]
    assert r == checksum(b"\x00" * MiB)


@threads_params
def test_blkhash_hole_sha256(threads):
    r = bench.blkhash(
        input_type="hole",
        input_size=INPUT_SIZE,
        digest_name=DIGEST,
        threads=threads,
        cool_down=0,
    )["checksum"]
    assert r == checksum(b"\x00" * MiB)


@threads_params
def test_blkhash_data_null(threads):
    r = bench.blkhash(
        input_type="data",
        input_size=INPUT_SIZE,
        digest_name="null",
        threads=threads,
        cool_down=0,
    )["checksum"]
    assert r == ""


@threads_params
def test_blkhash_zero_null(threads):
    r = bench.blkhash(
        input_type="zero",
        input_size=INPUT_SIZE,
        digest_name="null",
        threads=threads,
        cool_down=0,
    )["checksum"]
    assert r == ""


@threads_params
def test_blkhash_hole_null(threads):
    r = bench.blkhash(
        input_type="hole",
        input_size=INPUT_SIZE,
        digest_name="null",
        threads=threads,
        cool_down=0,
    )["checksum"]
    assert r == ""


def test_digest_sha256():
    r = bench.digest(
        digest_name=DIGEST,
        input_size=INPUT_SIZE,
        cool_down=0,
    )["checksum"]
    assert r == "dab852c11ae8f79aa478e168d108ee88a49c1c1bc7fd2154833a9fbfeb46de28"


def test_digest_null():
    r = bench.digest(
        digest_name="null",
        input_size=INPUT_SIZE,
        cool_down=0,
    )["checksum"]
    assert r == ""


def checksum(data):
    h = blkhash.Blkhash(DIGEST)
    h.update(data)
    return h.hexdigest()
