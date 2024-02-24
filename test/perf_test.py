# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import perf

# NOTE: Seems that the behavior changed in perf-6.7.7-200.fc39.x86_64; before
# specying "cycles" returns "cycles:u", and after specifying "cycles"
# returns "cycles". Make the test work with both behaviours.


def test_stat():
    stats, stdout = perf.stat(["true"])
    assert stdout == None

    cycles = stats.get("cycles") or stats.get("cycles:u")

    assert type(cycles["counter-value"]) is float
    assert type(cycles["metric-value"]) is float


def test_stat_events_short_names():
    stats, _ = perf.stat(["true"], events=("cycles", "instructions"))
    assert list(stats) == ["cycles", "instructions"] or ["cycles:u", "instructions:u"]


def test_stat_events_full_names():
    stats, _ = perf.stat(["true"], events=("cycles:u", "instructions:u"))
    assert list(stats) == ["cycles:u", "instructions:u"]


def test_stat_stdout():
    stats, stdout = perf.stat(["echo", "-n", "out"], capture_stdout=True)
    assert stdout == b"out"
