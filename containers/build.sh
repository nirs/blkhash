#!/bin/sh -e

# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

tag=${1:?Usage build tag}
image=blkhash-test:$tag

podman build --tag quay.io/nirsof/$image \
    --platform linux/amd64 \
    -f $tag.containerfile \
    .

podman push  quay.io/nirsof/$image
