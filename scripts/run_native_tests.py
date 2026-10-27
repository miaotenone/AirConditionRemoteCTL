#!/usr/bin/env python3
"""Build and run all native suites; stop on the first compiler/test failure."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUITES = {
    "ac_encoder": ("ac_encoder.cpp",),
    "ac_state": ("ac_state.cpp", "ac_encoder.cpp"),
    "wifi_policy": ("wifi_policy.cpp",),
    "link_guard": ("link_guard.cpp",),
    "reset_info": ("reset_info.cpp",),
    "failsafe": ("failsafe.cpp",),
}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"))
    parser.add_argument(
        "--compiler-arg",
        action="append",
        default=[],
        help="extra compiler argument (repeatable; use = for an argument starting with -)",
    )
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "native")
    args = parser.parse_args(argv)
    compiler = shutil.which(args.compiler)
    if compiler is None:
        print(
            f"C++ compiler not found: {args.compiler!r}. Install g++/clang++ "
            "and add it to PATH, or set CXX / --compiler to its executable path.",
            file=sys.stderr,
        )
        return 2

    try:
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        suffix = ".exe" if os.name == "nt" else ""
        for name, sources in SUITES.items():
            print(f"== {name} ==", flush=True)
            executable = build / f"{name}_test{suffix}"
            # A rejected compilation must never fall through to yesterday's binary.
            executable.unlink(missing_ok=True)
            command = [
                compiler,
                *args.compiler_arg,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests" / "native" / f"{name}_test.cpp"),
                *(str(ROOT / "src" / source) for source in sources),
                "-o",
                str(executable),
            ]
            subprocess.run(command, cwd=ROOT, check=True)
            subprocess.run([str(executable)], cwd=ROOT, check=True)
    except subprocess.CalledProcessError as error:
        print(f"Native suite failed (exit {error.returncode}).", file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1
    except OSError as error:
        print(f"Could not build/run native tests: {error}", file=sys.stderr)
        return 2
    print(f"All {len(SUITES)} native suites passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
