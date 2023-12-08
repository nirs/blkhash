<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# blkhash portability

The `blkhash` library and `blksum` command are developed on Linux, but
should be portable to any platform where openssl is available. Some
optimizations are implemented only for Linux.

The `blksum` command requires
[libnbd](https://libguestfs.org/libnbd.3.html) for `NBD` support, and
[qemu-nbd](https://www.qemu.org/docs/master/tools/qemu-nbd.html) for
`qcow2` format support. If `libnbd` is not available, `blksum` is built
without `NBD` support and can be used only with `raw` images.

## Testing status

Only some variants have CI. Most variants are tested only in `copr`
build system or have no automated testing.

| OS                | Arch          | CI                | libnbd |
|-------------------|---------------|-------------------|--------|
| Fedora 37         | x86_64        | gitlab, circleci  | yes    |
| Fedora 38         | x86_64        | gitlab, circleci  | yes    |
| Fedora 39         | x86_64        | gitlab, circleci  | yes    |
| Fedora 40         | x86_64        | gitlab, circleci  | yes    |
| CentOS Stream 8   | x86_64        | gitlab, circleci  | yes    |
| CentOS Stream 9   | x86_64        | gitlab, circleci  | yes    |
| Fedora 37         | aarch64       | copr              | yes    |
| Fedora 37         | s390x         | copr              | yes    |
| Fedora 37         | x86_64        | copr              | yes    |
| Fedora 38         | aarch64       | copr              | yes    |
| Fedora 38         | s390x         | copr              | yes    |
| Fedora 38         | x86_64        | copr              | yes    |
| Fedora 39         | aarch64       | copr              | yes    |
| Fedora 39         | s390x         | copr              | yes    |
| Fedora 39         | x86_64        | copr              | yes    |
| Fedora 40         | aarch64       | copr              | yes    |
| Fedora 40         | s390x         | copr              | yes    |
| Fedora 40         | x86_64        | copr              | yes    |
| CentOS Stream 8   | aarch64       | copr              | yes    |
| CentOS Stream 8   | x86_64        | copr              | yes    |
| CentOS Stream 9   | aarch64       | copr              | yes    |
| CentOS Stream 9   | s390x         | copr              | yes    |
| CentOS Stream 9   | x86_64        | copr              | yes    |
| RHEL 8            | aarch64       | copr              | yes    |
| RHEL 8            | s390x         | copr              | yes    |
| RHEL 8            | x86_64        | copr              | yes    |
| RHEL 9            | aarch64       | copr              | yes    |
| RHEL 9            | s390x         | copr              | yes    |
| RHEL 9            | x86_64        | copr              | yes    |
| RHEL 8.7          | x86_64        | no                | yes    |
| FreeBSD 13        | x86_64        | no                | no     |
| macOS 13 Ventura  | Apple Silicon | no                | no     |
