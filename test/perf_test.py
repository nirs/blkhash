# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import perf


def test_stat():
    stats, stdout = perf.stat(["true"])
    assert stdout == None
    cycles = stats["cycles:u"]
    assert type(cycles["counter-value"]) is float
    assert type(cycles["metric-value"]) is float


def test_stat_events_short_names():
    stats, _ = perf.stat(["true"], events=("cycles", "instructions"))
    assert list(stats) == ["cycles:u", "instructions:u"]


def test_stat_events_full_names():
    stats, _ = perf.stat(["true"], events=("cycles:u", "instructions:u"))
    assert list(stats) == ["cycles:u", "instructions:u"]


def test_stat_stdout():
    stats, stdout = perf.stat(["echo", "-n", "out"], capture_stdout=True)
    assert stdout == b"out"
