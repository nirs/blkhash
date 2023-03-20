# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import bench


def test_blkhash_data_sha256():
    r = bench.blkhash("data", "1m", digest_name="sha256")["checksum"]
    assert r == "dbf81934fc3cddca0aa816270d4d673a0e7095ea62d7174ae13895fc9fd0fb33"


def test_blkhash_zero_sha256():
    r = bench.blkhash("zero", "1m", digest_name="sha256")["checksum"]
    assert r == "b36c68ab266e8b0016406f655794d433910c7e3eae0b08fe7dccb5f46af9a5e8"


def test_blkhash_hole_sha256():
    r = bench.blkhash("hole", "1m", digest_name="sha256")["checksum"]
    assert r == "b36c68ab266e8b0016406f655794d433910c7e3eae0b08fe7dccb5f46af9a5e8"


def test_blkhash_data_null():
    r = bench.blkhash("data", "1m", digest_name="null")["checksum"]
    assert r == ""


def test_blkhash_zero_null():
    r = bench.blkhash("zero", "1m", digest_name="null")["checksum"]
    assert r == ""


def test_blkhash_hole_null():
    r = bench.blkhash("hole", "1m", digest_name="null")["checksum"]
    assert r == ""


def test_openssl_sha256():
    r = bench.openssl("1m", digest_name="sha256")["checksum"]
    assert r == "dab852c11ae8f79aa478e168d108ee88a49c1c1bc7fd2154833a9fbfeb46de28"


def test_openssl_null():
    r = bench.openssl("1m", digest_name="null")["checksum"]
    assert r == ""
