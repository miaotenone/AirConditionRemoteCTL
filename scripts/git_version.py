"""Stamp the firmware build with the git revision (see src/version.h).

PlatformIO loads this before compiling (``extra_scripts`` in platformio.ini), so
the boot banner can name the exact build instead of just saying "boot" -- the
one question a running unit otherwise cannot answer.

Deliberately forgiving: a tree without git (or with git missing) still has to
build, so every git call falls back to "unknown" rather than failing the build.
"""

import subprocess

Import("env")  # noqa: F821 - provided by PlatformIO


def _git(*args):
    """Run one git command in the project dir; "" when anything goes wrong."""
    try:
        out = subprocess.check_output(("git",) + args,
                                      cwd=env["PROJECT_DIR"],
                                      stderr=subprocess.DEVNULL)
    except Exception:
        return ""
    return out.decode("utf-8", "replace").strip()


def _revision():
    revision = _git("rev-parse", "--short", "HEAD")
    if not revision:
        return "unknown"
    # Uncommitted changes mean the binary is not the commit it names.
    if _git("status", "--porcelain"):
        revision += "-dirty"
    return revision


env.Append(BUILD_FLAGS=['-DFW_GIT_HASH=\\"%s\\"' % _revision()])
