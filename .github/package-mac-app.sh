#!/bin/bash
set -euo pipefail

usage() {
	echo "usage: $0 --binary BINARY --app APP" >&2
	exit 2
}

BINARY=
APP=
while [ "$#" -gt 0 ]; do
	case "$1" in
		--binary)
			[ "$#" -ge 2 ] || usage
			BINARY="$2"
			shift 2
			;;
		--app)
			[ "$#" -ge 2 ] || usage
			APP="$2"
			shift 2
			;;
		*)
			usage
			;;
	esac
done

[ -n "$BINARY" ] || usage
[ -n "$APP" ] || usage
[ -f "$BINARY" ] || { echo "missing binary: $BINARY" >&2; exit 1; }

ROOT_DIR="$(pwd)"
BINARY="$(cd "$(dirname "$BINARY")" && pwd -P)/$(basename "$BINARY")"
CONTENTS="$APP/Contents"
MACOS="$CONTENTS/MacOS"
FRAMEWORKS="$CONTENTS/Frameworks"
RESOURCES="$CONTENTS/Resources"
APP_EXE="$MACOS/sim"

FREETYPE_PREFIX="$(brew --prefix freetype)"
PNG_PREFIX="$(brew --prefix libpng)"
ZSTD_PREFIX="$(brew --prefix zstd)"
SDL3_PREFIX="$(brew --prefix sdl3)"

FREETYPE_DYLIB="$FREETYPE_PREFIX/lib/libfreetype.6.dylib"
PNG_DYLIB="$PNG_PREFIX/lib/libpng16.16.dylib"
ZSTD_DYLIB="$ZSTD_PREFIX/lib/libzstd.1.dylib"
SDL3_DYLIB="$SDL3_PREFIX/lib/libSDL3.0.dylib"

for dylib in "$FREETYPE_DYLIB" "$PNG_DYLIB" "$ZSTD_DYLIB" "$SDL3_DYLIB"; do
	[ -f "$dylib" ] || { echo "missing dylib: $dylib" >&2; exit 1; }
done

rm -rf "$APP"
mkdir -p "$MACOS" "$FRAMEWORKS" "$RESOURCES"
cp -p "$BINARY" "$APP_EXE"
cp -p "$ROOT_DIR/OSX/Info.plist" "$CONTENTS/Info.plist"
cp -p "$ROOT_DIR/OSX/simutrans.icns" "$RESOURCES/simutrans.icns"
printf 'APPL????\n' > "$CONTENTS/PkgInfo"

cp -p "$FREETYPE_DYLIB" "$FRAMEWORKS/libfreetype.6.dylib"
cp -p "$PNG_DYLIB" "$FRAMEWORKS/libpng16.16.dylib"
cp -p "$ZSTD_DYLIB" "$FRAMEWORKS/libzstd.1.dylib"
cp -p "$SDL3_DYLIB" "$FRAMEWORKS/libSDL3.0.dylib"

install_name_tool \
	-change "$FREETYPE_DYLIB" "@executable_path/../Frameworks/libfreetype.6.dylib" \
	-change "$PNG_DYLIB" "@executable_path/../Frameworks/libpng16.16.dylib" \
	-change "$ZSTD_DYLIB" "@executable_path/../Frameworks/libzstd.1.dylib" \
	-change "$SDL3_DYLIB" "@executable_path/../Frameworks/libSDL3.0.dylib" \
	"$APP_EXE"

install_name_tool \
	-change "$PNG_DYLIB" "@executable_path/../Frameworks/libpng16.16.dylib" \
	"$FRAMEWORKS/libfreetype.6.dylib"

install_name_tool -id "@executable_path/../Frameworks/libfreetype.6.dylib" "$FRAMEWORKS/libfreetype.6.dylib"
install_name_tool -id "@executable_path/../Frameworks/libpng16.16.dylib" "$FRAMEWORKS/libpng16.16.dylib"
install_name_tool -id "@executable_path/../Frameworks/libzstd.1.dylib" "$FRAMEWORKS/libzstd.1.dylib"
install_name_tool -id "@loader_path/libSDL3.0.dylib" "$FRAMEWORKS/libSDL3.0.dylib"

codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

echo "Created $APP"
