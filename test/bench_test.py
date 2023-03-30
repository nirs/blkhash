# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import pytest
import bench

threads_params = pytest.mark.parametrize("threads", [1, 2, 4, 8, 16, 32])


@threads_params
def test_blkhash_data_sha256(threads):
    r = bench.blkhash(
        input_type="data",
        input_size="1m",
        digest_name="sha256",
        threads=threads,
    )["checksum"]
    assert r == "f596205d1108c4752339b76f7a046fc9b40ed096c393cc1a9a32b052f679eef6"


@threads_params
def test_blkhash_zero_sha256(threads):
    r = bench.blkhash(
        input_type="zero",
        input_size="1m",
        digest_name="sha256",
        threads=threads,
    )["checksum"]
    assert r == "146db9301db0c190f877b31668c71f9eeedd23b76b0349e66e8d04ad023b2cd2"


@threads_params
def test_blkhash_hole_sha256(threads):
    r = bench.blkhash(
        input_type="hole",
        input_size="1m",
        digest_name="sha256",
        threads=threads,
    )["checksum"]
    assert r == "146db9301db0c190f877b31668c71f9eeedd23b76b0349e66e8d04ad023b2cd2"


@threads_params
def test_blkhash_data_null(threads):
    r = bench.blkhash(
        input_type="data",
        input_size="1m",
        digest_name="null",
        threads=threads,
    )["checksum"]
    assert r == ""


@threads_params
def test_blkhash_zero_null(threads):
    r = bench.blkhash(
        input_type="zero",
        input_size="1m",
        digest_name="null",
        threads=threads,
    )["checksum"]
    assert r == ""


@threads_params
def test_blkhash_hole_null(threads):
    r = bench.blkhash(
        input_type="hole",
        input_size="1m",
        digest_name="null",
        threads=threads,
    )["checksum"]
    assert r == ""


def test_openssl_sha256():
    r = bench.openssl("1m", digest_name="sha256")["checksum"]
    assert r == "dab852c11ae8f79aa478e168d108ee88a49c1c1bc7fd2154833a9fbfeb46de28"


def test_openssl_null():
    r = bench.openssl("1m", digest_name="null")["checksum"]
    assert r == ""
