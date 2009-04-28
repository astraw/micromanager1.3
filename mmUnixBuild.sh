#!/bin/bash
# enable error handling
set -e

# bootstrap autotools
aclocal
libtoolize --force
automake --foreign --add-missing
autoconf
cd DeviceAdapters
aclocal
libtoolize --force
automake --foreign --add-missing
autoconf
cd ..
