#!/bin/sh

# NOTE: Run mingw32-build.sh first. You may also want to comment
# out the last line "sigre ...".

# Create cclive-$release.zip package.

# Edit these as needed:
CURL_PATH="/usr/home/legatvs/src/curl-7.19.5"
CURL_DLL="$CURL_PATH/dist/bin/libcurl-4.dll"
CURL_COPYING="$CURL_PATH/COPYING"

ICONV_PATH="/usr/home/legatvs/src/libiconv-1.13.1"
ICONV_DLL="$ICONV_PATH/dist/bin/libiconv-2.dll"
ICONV_COPYING="$ICONV_PATH/COPYING.LIB"

VERSION=`awk '/PACKAGE_VERSION = / {print $3}' Makefile`
ARCHIVE="cclive-$VERSION-win32.zip"
DISTDIR="cclive-$VERSION"

rm -rf dist cclive-$VERSION $ARCHIVE

make install-strip \
&& make man \
&& cp $CURL_DLL dist/bin \
&& cp $ICONV_DLL dist/bin \
&& cp ChangeLog dist/ChangeLog.txt \
&& cp COPYING dist/COPYING.txt \
&& cp $CURL_COPYING dist/COPYING.cURL.txt \
&& cp $ICONV_COPYING dist/COPYING.LIB.iconv.txt \
&& cp README dist/README.txt \
&& cp README.w32 dist/README.w32.txt \
&& cp cclive.1.html dist \
&& rm -r dist/share \
&& mv dist $DISTDIR \
&& zip -9 -r $ARCHIVE $DISTDIR

exit $?

