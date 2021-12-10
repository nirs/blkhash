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
        python3-pip \
        qemu-img \
    && dnf clean all

# Install pytest with pip.
RUN python3 -m pip install --upgrade pip \
    && python3 -m pip install --upgrade pytest
