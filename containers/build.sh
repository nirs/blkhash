#!/bin/sh

# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

podman build -t blkhash-$1 -f $1.containerfile .
