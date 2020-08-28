#!/bin/sh
# builds for ubuntu

# normal build
echo "BACKEND = sdl2" >config.default
echo "OSTYPE = linux" >>config.default
echo "COLOUR_DEPTH =16" >>config.default
echo "DEBUG = 0" >>config.default
echo "MSG_LEVEL = 3" >>config.default
echo "OPTIMISE = 1" >>config.default
echo "MULTI_THREAD = 1" >>config.default
echo "STATIC = 1" >>config.default
echo "USE_ZSTD = 1" >>config.default
echo "USE_FREETYPE = 1" >>config.default
echo "WITH_REVISION = $(svn info --show-item revision svn://servers.simutrans.org/simutrans)" >>config.default
echo "FLAGS = -std=c++11" >>config.default
make -j2
mv build/default/sim sim-linux-OTRP
