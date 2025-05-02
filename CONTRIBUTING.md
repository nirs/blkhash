# Contributing

Contribution is welcome!

Please check issues with the
[good first_issue](https://gitlab.com/nirs/blkhash/-/issues?sort=priority&state=opened&label_name[]=good+first+issue)
for issues that can be easier to start with.

Please [create a new issue](https://gitlab.com/nirs/blkhash/-/issues/new)
for new features you want to work on.

See the next sections on how to set up a development environment, run
the tests and debug.

## Setting up development environment

### Fedora

Fedora 38 or later is required for development.

Install required packages:

    sudo dnf install \
        asciidoc \
        blake3 \
        blake3-devel \
        gcc \
        git \
        libnbd-devel \
        meson \
        openssl-devel \
        perf \
        python3 \
        python3-pytest \
        qemu-img \
        reuse \
        rpm-build \
        rpmlint

### Ubuntu

Tested with Ubuntu 22.04.

    sudo apt update

    sudo apt install \
        asciidoc \
        cmake \
        gcc \
        git \
        libnbd-dev \
        meson \
        libssl-dev \
        pkg-config \
        python3 \
        python3-pytest \
        qemu-utils \
        reuse

#### Installing blake3

blake3 is not packaged for Ubuntu yet, so you need to build and install
it from source.

> IMPORTANT: Don't forget to use Release build type. Debug build can be
> 25 times slower.

```sh
git clone https://github.com/BLAKE3-team/BLAKE3.git
cd BLALE3
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=true ../c
make
sudo make install
```

#### Installing perf

Install the linux-tools package for your kernel:

```sh
sudo apt-get install linux-tools-$(uname -r)
```

Allow access to perf events:

```sh
sudo sh -c 'echo -1 > /proc/sys/kernel/perf_event_paranoid'
```

You may wan to consider a more secure setup.

### FreeBSD

FreeBSD 13 or later is required.

Install required packages:

    pkg install \
        asciidoc \
        git \
        meson \
        openssl \
        pkgconf \
        python3 \
        qemu-utils

Install pytest and reuse via pip. See the section "Creating python
virtual environment" below.

### macOS

The recommended way to get the required packages is the
[Homebrew](https://brew.sh/).
You can use [MacPorts](https://www.macports.org/), but more work is
needed and there are some issues.

These instructions were tested on *macOS Ventura 13.1*.

#### Using Homebrew

Install required packages:

    brew install \
        asciidoc \
        blake3 \
        docbook-xsl \
        meson \
        openssl \
        pkg-config \
        qemu

Add these vars to `~/.zprofile`:

    # Allow pkg-config to find homebew configs
    export PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig:/opt/homebrew/opt/blake3/lib/pkgconfig"

    # Allow xmllint to find homebrew catalogs
    export XML_CATALOG_FILES=/opt/homebrew/etc/xml/catalog

Open a new shell to apply the changes.

Install pytest and resuse via pip. See the section "Creating virtual
environment" below.

#### Using MacPorts

Install require packages:

    port install \
        asciidoc \
        meson \
        openssl \
        pkgconfig \
        py311-certifi \
        py311-pytest \
        qemu \
        reuse

Select python3 and pytest versions:

    port select --set python3 python311
    port select --set pytest pytest311

Find certifi ca file:

    % certifi_ca=$(python3 -c 'import certifi; print(certifi.where())')

Find ssl openssl_cafile location:

    % ssl_ca=$(python3 -c 'import ssl; print(ssl.get_default_verify_paths().openssl_cafile)')

Backup ssl ca file:

    mv $ssl_ca $ssl_ca.bak

Link to certifi ca file to ssl ca file:

    ln -s $certifi_ca $ssl_ca

Install pytest and reuse via pip. See the section "Creating python
virtual environment" below.

### Creating python virtual environment

If `pytest` or *reuse* are not available using your package manager, you
can install them using pip in a virtual environment:

    python3 -m venv ~/.venv/blkhash
    source ~/.venv/blkhash/bin/activate
    pip install pytest reuse
    deactivate

On FreeBSD using the default shell (sh), use `.` instead of `source`:

    . ~/.venv/blkhash/bin/activate

To use the virtual environment for configuring, building or running the
tests, activate it:

    source ~/venv/blkhash/bin/activate

When you are done, you can deactivate the virtual environment:

    deactivate

## Get the source

    git clone https://gitlab.com/nirs/blkhash.git

## Configuring

Create a build directory with default options:

    meson setup build

When building on *macOS* via *MacPorts*, you need to skip building the
manual pages:

    meson setup build -Dman=disabled

The default options:

- nbd=auto - Support `NBD` if `libnbd` is available.

To configure build directory for release installing in /usr:

    meson configure build --buildtype=release --prefix=/usr

To see all available options and possible values (after running meson
setup):

    meson configure build

## Building

To build run:

    meson compile -C build

Instead of specifying the directory, you can run the command inside the
build directory:

    cd build
    meson compile

## Installing

To install at configured --prefix:

    meson install -C build

## Building rpms

Before building rpms you need to run meson setup.

To build source rpm in build/rpm/SRPMS run:

    ./build-srpm

To build rpms in build/rpm/RPMS/{arch} run:

    ./build-rpms

## Debugging

To view debug logs run with:

    BLKSUM_DEBUG=1 blksum disk.img

To skip checksum calculation for testing I/O performance:

    BLKSUM_IO_ONLY=1 blksum disk.img

## Running the tests

To run all tests:

    meson test -C build

Instead of specifying the directory, you can run the command inside the
build directory:

    cd build
    meson test

To see verbose test output use:

    meson test -C build -v

To run specific `blksum` tests, use `pytest` directly:

    meson compile -C build
    pytest -k sha1-sparse

If `blksum` is built with `NBD` support, enable the `NBD` tests:

    HAVE_NBD=1 pytest -k sha1-sparse

`pytest` uses the `build` directory by default. If you want to use
another directory name, or installed `blksum` executable, specify the
path to the executable in the environment:

    meson setup release --buildtype=release
    meson compile -C release
    BLKSUM=release/bin/blksum pytest

To run only `blkhash` tests:

   meson compile -C build
   build/test/blkhash_test
