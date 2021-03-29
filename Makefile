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

src = blkhash.c blksum.c file.c pipe.c nbd.c

CFLAGS = -g -O3 -Wall -Werror
LDLIBS = -lcrypto -lnbd

TEST_CFLAGS = -I./unity/src -g -Wall -Werror
TEST_LDLIBS = -lcrypto

obj = $(src:.c=.o)
dep = $(src:.c=.d)

.PHONY: test clean

blksum: $(obj)
	$(LINK.o) $^ $(LDLIBS) -o $@

test: blksum blkhash_test
	pytest -v
	./blkhash_test

blkhash_test: blkhash.c blkhash_test.c ./unity/src/unity.c
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDLIBS)

clean:
	rm -f blksum blkhash_test *.[do]

%.o: %.c %.d
	$(COMPILE.c) $< -MMD -MF $*.d -o $@

%.d: ;

-include $(dep)
