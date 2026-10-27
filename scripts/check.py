#!/usr/bin/env python3
"""Run repository quality checks without opening serial ports or using credentials."""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def text_errors(paths):
    """Report UTF-8, final-newline and trailing-whitespace policy violations."""
    errors = []
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            errors.append(f"{path}: not UTF-8")
            continue
        if text.startswith("\ufeff"):
            errors.append(f"{path}: remove UTF-8 BOM")
        if text and not text.endswith("\n"):
            errors.append(f"{path}: missing final newline")
        for line_number, line in enumerate(text.splitlines(), 1):
            if line != line.rstrip(" \t"):
                errors.append(f"{path}:{line_number}: trailing whitespace")
    return errors


def run(*command):
    print("+ " + " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=ROOT, check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="apply C++/Python format fixes")
    parser.add_argument("--format-only", action="store_true", help="skip native/Python tests")
    parser.add_argument(
        "--build", action="store_true", help="also compile both firmware environments"
    )
    args = parser.parse_args(argv)
    cpp = sorted((ROOT / "src").glob("*.cpp")) + sorted((ROOT / "src").glob("*.h"))
    cpp += sorted((ROOT / "tests" / "native").rglob("*.cpp"))
    cpp += sorted((ROOT / "tests" / "native").rglob("*.h"))
    python = [ROOT / "scripts", ROOT / "tests" / "python"]
    try:
        # Using this interpreter's package also works when Scripts/ is not on PATH.
        run(
            sys.executable,
            "-c",
            "import clang_format; clang_format.clang_format()",
            *(["-i"] if args.fix else ["--dry-run", "--Werror"]),
            *cpp,
        )
        # Expand legacy one-line statements first: Ruff lint cannot auto-fix
        # E701, but the formatter can. Import fixes can then be formatted again.
        if args.fix:
            run(sys.executable, "-m", "ruff", "format", *python)
        run(sys.executable, "-m", "ruff", "check", *(["--fix"] if args.fix else []), *python)
        run(sys.executable, "-m", "ruff", "format", *([] if args.fix else ["--check"]), *python)
        paths = cpp + [path for folder in python for path in folder.glob("*.py")]
        paths += [ROOT / "platformio.ini", ROOT / "requirements-dev.txt", ROOT / "pyproject.toml"]
        errors = text_errors(paths)
        if errors:
            print("\n".join(errors), file=sys.stderr)
            return 1
        if not args.format_only:
            run(sys.executable, ROOT / "scripts" / "run_native_tests.py")
            run(sys.executable, "-m", "unittest", "discover", "-s", "tests/python", "-v")
        if args.build:
            run(sys.executable, "-m", "platformio", "run", "-e", "esp32dev", "-e", "selftest")
    except subprocess.CalledProcessError as error:
        print(
            "Quality gate failed. Install requirements-dev.txt for missing tools; "
            "use python scripts/check.py --fix for formatting violations.",
            file=sys.stderr,
        )
        return error.returncode if error.returncode > 0 else 1
    except OSError as error:
        print(f"Quality gate could not run: {error}", file=sys.stderr)
        return 2
    print("Quality gate passed (no hardware validation).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
