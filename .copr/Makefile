# SPDX-FileCopyrightText: Red Hat Inc
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# The makefile is called as:
# make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
#
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm

srpm:
	# Unfortunately we need most of the build requirements to create source
	# rpm, since we must setup meson to use meson dist, and setting up meson
	# checks the required pakcages and libraries.
	# TODO: Minimize requirements for creating source rpm.
	dnf install -y \
		asciidoc \
		gcc \
		git \
		libnbd-devel \
		meson \
		openssl-devel \

	meson setup build
	./build-srpm
	mv build/rpm/SRPMS/*.src.rpm $(outdir)
