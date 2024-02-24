# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import json
import subprocess


def stat(args, detailed=0, events=(), capture_stdout=False):
    """
    Run a command and gather performance counter statistics.

    Combine json lines output from perf stat into an esaier to use dict.

    Arguments:
      detailed(int): Detail level (0-3)
      events(iterable): If set, return only specified events names, otherwise
        return default events.  See `man perf-list` for more info.
      capture_stdout(bool): If True capture command stdout and return it.

    Return tuple (stats, stdout). stats is a dict of supported event values.
    """
    cmd = ["perf", "stat", "--json-output"]
    if detailed:
        cmd.append("-" + "d" * detailed)
    if events:
        cmd.append("--event")
        cmd.append(",".join(events))
    cmd.extend(args)
    stdout = subprocess.PIPE if capture_stdout else None
    cp = subprocess.run(cmd, stderr=subprocess.PIPE, stdout=stdout, check=True)
    stats = parse_json_lines(cp.stderr)
    return stats, cp.stdout


def parse_json_lines(data):
    res = {}
    for line in data.decode().splitlines():
        # Example line:
        # {"counter-value" : "96391.000000", "unit" : "", "event" : "cycles:u",
        # "event-runtime" : 556792, "pcnt-running" : 100.00, "metric-value" :
        # "0.173119", "metric-unit" : "GHz"}
        value = json.loads(line)

        # Drop unsupported events.
        # {"counter-value" : "<not supported>", "unit" : "", "event" :
        # "branches:u", "event-runtime" : 0, "pcnt-running" : 100.00,
        # "metric-value" : "0.000000", "metric-unit" : ""}
        if value["counter-value"] in ("<not supported>", "<not counted>"):
            continue

        # Convert string values to numbers.
        for key in "counter-value", "metric-value":
            value[key] = float(value[key])

        event = value.pop("event")
        res[event] = value

    return res
