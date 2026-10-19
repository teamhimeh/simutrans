#!/usr/bin/env python3
"""Compile focused regression checks from the real keybinding/file functions.

Run with Python 3 and a C++17 compiler, e.g. --cxx g++. No pakset is needed.
The harness links the real Unicode decoder and extracts the production functions
so the failure-cleanup and UTF-8 tests exercise the implementation being reviewed.
"""
import argparse
from pathlib import Path
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
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    menu = (root / "simmenu.cc").read_text(encoding="utf-8")
    system = (root / "sys/simsys.cc").read_text(encoding="utf-8")
    strings = (root / "utils/simstring.cc").read_text(encoding="utf-8")
    parts = [r'''#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <cassert>
#include <string>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include "unicode.h"
#include "simevent.h"
#ifdef _WIN32
#include <windows.h>
void dr_fatal_notify(const char *) { std::abort(); }
''', definition(system, "class U16View") + ";\n#endif"]
    for signature in ["FILE *dr_fopen (", "int dr_stat(", "int dr_remove(", "int dr_rename("]:
        parts.append(definition(system, signature))
    parts.extend([
        definition(strings, "char const* strstart("),
        definition(menu, "static utf32 str_to_key("),
        "std::string shortcut_menu_path, shortcut_default_path;",
        definition(menu, "bool read_shortcut_file("),
        r'''bool fail_write = false;
size_t checked_fwrite(const void *data, size_t size, size_t count, FILE *file) {
    return fail_write ? 0 : std::fwrite(data, size, count, file);
}
#define fwrite checked_fwrite
''',
        definition(menu, "bool write_shortcut_file("),
        "#undef fwrite",
        definition(menu, "bool ensure_shortcut_backup()"),
        definition(menu, "size_t shortcut_field("),
        definition(menu, "unsigned shortcut_key_field("),
        definition(menu, "std::pair<uint16, uint8> shortcut_from_value("),
        definition(menu, "std::string shortcut_to_value("),
        r'''int main() {
    for (const char *name : {"ascii", u8"日本語のフォルダ"}) {
        const auto dir = std::filesystem::u8path(name);
        std::filesystem::create_directories(dir);
        shortcut_menu_path = (dir / "menuconf.tab").u8string();
        shortcut_default_path = (dir / "menuconf.default.tab").u8string();
        const std::string original = u8"# 日本語の説明\r\ngeneral_tool[0]=0,0,9,あ\r\n";
        assert(write_shortcut_file(shortcut_menu_path, original));
        assert(ensure_shortcut_backup());
        std::string data;
        assert(read_shortcut_file(shortcut_default_path, data) && data == original);
        const auto backup = std::filesystem::u8path(shortcut_default_path);
        const auto time = std::filesystem::last_write_time(backup);
        assert(write_shortcut_file(shortcut_menu_path, "changed"));
        assert(ensure_shortcut_backup());
        assert(read_shortcut_file(shortcut_default_path, data) && data == original);
        assert(std::filesystem::last_write_time(backup) == time);

        // Failed writes and failed replacement must both remove the UTF-8 .tmp path.
        fail_write = true;
        assert(!write_shortcut_file(shortcut_menu_path, "not saved"));
        fail_write = false;
        assert(!std::filesystem::exists(std::filesystem::u8path(shortcut_menu_path + ".tmp")));
        assert(read_shortcut_file(shortcut_menu_path, data) && data == "changed");
        const auto blocked = dir / "blocked.tab";
        std::filesystem::create_directory(blocked);
        assert(!write_shortcut_file(blocked.u8string(), "not saved"));
        assert(std::filesystem::is_directory(blocked));
        assert(!std::filesystem::exists(dir / "blocked.tab.tmp"));

        std::filesystem::remove(backup);
        std::filesystem::create_directory(backup);
        assert(!ensure_shortcut_backup());
        assert(std::filesystem::is_directory(backup));
    }
    uint8 flags;
    assert(str_to_key(u8"あ", &flags) == 0x3042 && flags == 0);
    assert(str_to_key(u8"あ,parameter", &flags) == 0x3042);
    assert(str_to_key(u8"漢 # 日本語", &flags) == 0x6f22);
    assert(str_to_key(u8"^あ", &flags) == 0x3042 && flags == 2);
    assert(str_to_key(u8"+あ", &flags) == 0x3042 && flags == 1);
    assert(str_to_key(u8"^+あ", &flags) == 0x3042 && flags == 3);
    assert(str_to_key(u8"§", &flags) == 0xa7);
    assert(str_to_key("a", &flags) == 'a');
    assert(str_to_key("^A", &flags) == 1 && flags == 2);
    assert(str_to_key("F15", &flags) == SIM_KEY_F15);
    assert(str_to_key("COMMA", &flags) == ',');
    for (uint8 modifiers = 0; modifiers < 4; modifiers++) {
        const auto value = "0,0,9," + shortcut_to_value(0x3042, modifiers);
        const auto key = shortcut_from_value("general_tool[0]", value);
        assert(key.first == 0x3042 && key.second == modifiers);
    }
    std::cout << "Unicode paths, cleanup failures, backup protection and UTF-8 keys: PASS\n";
}
'''])
    assert "ensure_shortcut_backup()" not in definition(menu, "bool tool_t::read_menu(")
    assert "ensure_shortcut_backup()" not in definition(menu, "bool save_shortcuts(")
    with tempfile.TemporaryDirectory(prefix="keybindings-unicode-") as temporary:
        directory = Path(temporary)
        source, binary = directory / "test.cc", directory / "test.exe"
        source.write_text("\n".join(parts), encoding="utf-8")
        subprocess.run([args.cxx, "-std=c++17", "-I", str(root), str(source),
                        str(root / "unicode.cc"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], cwd=directory, check=True)


if __name__ == "__main__":
    main()
