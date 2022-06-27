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
        python3-pytest \
        qemu-img \
        reuse \
        rpm-build \
        rpmlint \
    && dnf clean all
