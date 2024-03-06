# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later

FROM registry.fedoraproject.org/fedora:38

# Runtime packages.
RUN echo v1 \
    && dnf install -y \
        asciidoc \
        blake3-devel \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        procps-ng \
        python3 \
        python3-pytest \
        qemu-img \
        reuse \
        rpm-build \
        rpmlint \
    && dnf clean all
