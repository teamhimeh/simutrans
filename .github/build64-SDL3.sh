#!/bin/sh
set -e

# libbrotli static is broken in MinGW for freetype2
for f in libbrotlidec libbrotlienc libbrotlicommon; do
	mv "/mingw64/lib/$f.dll.a" "/mingw64/lib/$f.dll._"
	cp "/mingw64/lib/$f-static.a" "/mingw64/lib/$f.a"
done

echo "BACKEND = sdl3" >config.default
echo "OSTYPE = mingw" >>config.default
echo "DEBUG = 0" >>config.default
echo "MSG_LEVEL = 3" >>config.default
echo "OPTIMISE = 1" >>config.default
echo "MULTI_THREAD = 1" >>config.default
echo "USE_ZSTD = 1" >>config.default
echo "USE_FREETYPE = 1" >>config.default
echo "WITH_REVISION = 0" >>config.default
echo "FLAGS = -DREVISION=$(svn info --show-item revision svn://servers.simutrans.org/simutrans)" >>config.default
echo "STATIC = 1" >>config.default

make -j4
mv build/default/sim.exe sim-WinSDL3-OTRP.exe
