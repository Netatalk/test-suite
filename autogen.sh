#!/bin/sh

# build it all.
libtoolize --copy --force && \
	aclocal -I macros $ACLOCAL_FLAGS && \
	autoheader && \
	automake --include-deps --add-missing --foreign && \
	autoconf

# just in case automake generated errors...
autoconf

LDFLAGS=-rdynamic ./configure --enable-maintainer-mode -C "$@"
