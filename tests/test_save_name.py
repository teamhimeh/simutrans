#!/usr/bin/env python3
"""Build and run tests/save_name_test.cc against utils/save_filename.cc.

No pakset or game executable is needed. Run, for example:
    python tests/test_save_name.py --cxx g++
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    compiler = shutil.which(args.cxx) or args.cxx
    env = os.environ.copy()
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env["PATH"]
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "save_name_test.exe"
        subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-I", str(root),
                        str(root / "tests" / "save_name_test.cc"),
                        str(root / "utils" / "save_filename.cc"), "-o", str(exe)],
                       check=True, env=env)
        subprocess.run([str(exe)], check=True, env=env)


if __name__ == "__main__":
    main()
