#!/usr/bin/env python3
"""Build focused PNG heightmap checks; no pakset or game executable is needed.

Requires a C++11 compiler and libpng development files. Run, for example:
    python tests/test_heightmap_export.py --cxx g++
Compiler/library discovery uses pkg-config, with -lpng -lz as a fallback.
The exporter body, PNG codecs and height loader are production code. World data,
logging and the GUI text sink are adapters; injected close/rename failures act at
the I/O boundary. Filesystem wrappers, image classification and the filename
setter are extracted verbatim, like test_keybindings_unicode.py, to avoid linking
the game. This does not test world initialization, slope correction or GUI events.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


def definition(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    compiler = shutil.which(args.cxx) or args.cxx
    compiler_dir = Path(compiler).resolve().parent
    env = os.environ.copy()
    env["PATH"] = str(compiler_dir) + os.pathsep + env.get("PATH", "")
    pkg_config = shutil.which("pkg-config", path=env["PATH"])
    png_flags = ["-lpng", "-lz"]
    if pkg_config:
        result = subprocess.run([pkg_config, "--cflags", "--libs", "libpng"],
                                text=True, capture_output=True, env=env)
        if result.returncode == 0:
            png_flags = shlex.split(result.stdout)

    system = (root / "sys/simsys.cc").read_text(encoding="utf-8")
    classifier = (root / "io/classify_file.cc").read_text(encoding="utf-8")
    wrappers = ["#ifdef _WIN32", definition(system, "class U16View") + ";", "#endif"]
    for signature in ["FILE *dr_fopen (", "int dr_stat(", "int dr_remove("]:
        wrappers.append(definition(system, signature))
    wrappers.append(definition(system, "int dr_rename(").replace(
        "int dr_rename(", "int production_dr_rename(", 1))
    wrappers.append(definition(classifier, "file_info_t::file_info_t()"))
    for name in ["png", "bmp", "ppm"]:
        # Skip the forward declaration when locating the actual function body.
        signature = "bool classify_as_" + name + "(FILE *f, file_info_t *info)\n{"
        wrappers.append(definition(classifier, signature))
    wrappers.append(definition(classifier, "file_classify_status_t classify_image_file("))

    exporter = (root / "dataobj/height_map_exporter.cc").read_text(encoding="utf-8")
    # Replace the heavyweight world dependency, retaining every production method.
    assert exporter.count('#include "../simworld.h"') == 1
    exporter = exporter.replace('#include "../simworld.h"', "")
    exporter = re.sub(r'#include "([^"]+)"', lambda match:
                      '#include "' + (root / "dataobj" / match[1]).resolve().as_posix() + '"', exporter)
    strings = (root / "utils/simstring.cc").read_text(encoding="utf-8")
    savegame = (root / "gui/savegame_frame.cc").read_text(encoding="utf-8")
    filename_setter = "\n".join([definition(strings, "char *tstrncpy("),
                                  definition(strings, "char const* strstart("),
                                  definition(savegame, "void savegame_frame_t::set_filename(")])
    with tempfile.TemporaryDirectory(prefix="heightmap-export-") as temporary:
        directory = Path(temporary)
        (directory / "filesystem.inc").write_text("\n".join(wrappers), encoding="utf-8")
        (directory / "exporter.inc").write_text(exporter, encoding="utf-8")
        (directory / "filename_setter.inc").write_text(filename_setter, encoding="utf-8")
        # Fault injection changes only the I/O boundary, retaining the real codec.
        png_source = directory / "png_with_close_failure.cc"
        png_source.write_text('#include <cstdio>\n'
                              'extern int heightmap_test_fclose(FILE *);\n'
                              '#define fclose heightmap_test_fclose\n'
                              '#include "' + (root / "io/raw_image_png.cc").as_posix() + '"\n'
                              '#undef fclose\n', encoding="utf-8")
        binary = directory / ("test.exe" if os.name == "nt" else "test")
        sources = ["tests/heightmap_export_test.cc", "dataobj/height_map_loader.cc",
                   "io/raw_image.cc", "io/raw_image_bmp.cc",
                   "io/raw_image_ppm.cc", "simmem.cc", "simio.cc"]
        subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-I", str(root),
                        "-I", str(directory), *[str(root / source) for source in sources],
                        str(png_source),
                        "-o", str(binary), *png_flags], check=True, env=env)
        subprocess.run([str(binary)], cwd=directory, check=True, env=env)
        # Failed exporter operations must leave neither partial PNGs nor temps.
        assert sorted(path.name for path in directory.glob("*.tmp")) == []
        assert not (directory / "rename-failure.png").exists()
        assert not (directory / "日本語-失敗.png").exists()
        assert not (directory / "close-failure.png").exists()
        assert not (directory / "日本語-終了失敗.png").exists()


if __name__ == "__main__":
    main()
