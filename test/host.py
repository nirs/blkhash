# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

import os
import platform
import re
import subprocess


def info():
    """
    Return host info for benchmarks.
    """
    info = {
        "platform": platform.platform(),
        "online-cpus": os.sysconf("SC_NPROCESSORS_ONLN"),
        "cpu": cpu(),
    }
    if platform.system() == "Linux":
        info["smt"] = smt()
        try:
            info["tuned-profile"] = tuned_profile()
        except FileNotFoundError:
            pass
    return info


def smt():
    """
    Return Linux smt status.
    """
    with open("/sys/devices/system/cpu/smt/control") as f:
        return f.read().strip()


def cpu():
    """
    Return Linux cpu model name.
    """
    if platform.system() == "Darwin":
        cmd = ["sysctl", "-n", "machdep.cpu.brand_string"]
        cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
        return cp.stdout.decode().strip()
    else:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if re.match("model name\s*:", line):
                    return line.split(":", 1)[1].strip()


def tuned_profile():
    """
    Return Linux tuned profile name if tuned-adm is installed.
    """
    cmd = ["tuned-adm", "active"]
    cp = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    return cp.stdout.decode().split(":", 1)[1].strip()
