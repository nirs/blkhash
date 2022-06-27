# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

FROM quay.io/centos/centos:stream9

# Runtime packages.
RUN echo v1 \
    && dnf install -y \
        dnf-plugins-core \
    && dnf config-manager --set-enabled crb \
    && dnf install -y \
        asciidoc \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        python3 \
        python3-pytest \
        qemu-img \
        rpm-build \
        rpmlint \
    && dnf clean all
