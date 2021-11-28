FROM registry.fedoraproject.org/fedora:35

# Runtime packages.
RUN dnf install -y \
        asciidoc \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        python3 \
        python3-pip \
        qemu-img \
    && dnf clean all

# Install pytest with pip.
RUN python3 -m pip install --upgrade pip \
    && python3 -m pip install --upgrade pytest
