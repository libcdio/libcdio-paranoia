#!/bin/bash
# Upload script to put on ftp.gnu.org
#
#
libcdio_paranoia_owd=$(pwd)
cd $(dirname ${BASH_SOURCE[0]})
# LIBCDIO_VERSION=10.2+2.0.2
if [[ -n $LIBCDIO_VERSION ]]; then
    echo "Please set LIBCDIO_VERSION" 1>&2
    exit 1
fi

./gnupload --to ftp.gnu.org:libcdio ../dist/libcdio-paranoia-${LIBCIO_VERSION}.tar.gz ../dist/libcdio-paranoia-${LIBCDIO_VERSION}.tar.bz2
cd $libcdio_paranoia_owd
