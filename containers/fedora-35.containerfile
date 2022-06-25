# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

FROM registry.fedoraproject.org/fedora:35

# Runtime packages.
RUN echo v2 \
    && dnf install -y \
        asciidoc \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        python3 \
        python3-pip \
        qemu-img \
        rpm-build \
        rpmlint \
    && dnf clean all

# Install pytest with pip.
RUN python3 -m pip install --upgrade pip \
    && python3 -m pip install --upgrade pytest reuse
