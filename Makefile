# blkhash - block based checksum for disk images
# Copyright Nir Soffer <nirsof@gmail.com>.
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301 USA

CFLAGS = -g -O3 -Wall
LDLIBS = -lcrypto

.PHONY: clean test

blksum: blksum.o blkhash.o file.o pipe.o

blksum.o: blksum.c blkhash.h blksum.h

blkhash.o: blkhash.c blkhash.h

file.o: blksum.h

pipe.o: blksum.h

test: blksum
	pytest -v

clean:
	rm -f *.o blksum
