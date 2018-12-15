#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# Additional options go to configure.

# For OpenBSD
export AUTOCONF_VERSION=2.69
export AUTOMAKE_VERSION=1.15

echo "Rebuilding ./configure with autoreconf..."
autoreconf -f -i
if [ $? -ne 0 ]; then
  echo "autoreconf failed"
  exit $?
fi

./configure --enable-maintainer-mode "$@"
