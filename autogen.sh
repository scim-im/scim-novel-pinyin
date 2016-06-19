#!/bin/sh
autoheader
aclocal
libtoolize --copy --install
intltoolize --copy --automake --force
automake --add-missing --copy --force
autoconf
