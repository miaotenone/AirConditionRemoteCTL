"""Exercise the actual display translation unit without a board or I2C bus."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class DisplayTests(unittest.TestCase):
    def test_dashboard_layout_clipping_and_safety_labels(self):
        with tempfile.TemporaryDirectory(prefix="oled-layout-") as directory:
            executable = Path(directory) / ("display.exe" if os.name == "nt" else "display")
            subprocess.run(
                [
                    os.environ.get("CXX", "g++"),
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "tests/native/display_stubs"),
                    "-I",
                    str(ROOT / "src"),
                    str(ROOT / "tests/native/display_test.cpp"),
                    str(ROOT / "src/ui_navigation.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
