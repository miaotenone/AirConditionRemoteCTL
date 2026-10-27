"""Exercise the native runner with real subprocesses, never a board."""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "scripts" / "run_native_tests.py"


class NativeRunnerTests(unittest.TestCase):
    def test_helpers_honor_absolute_cxx_instead_of_default_on_path(self):
        compiler = shutil.which(os.environ.get("CXX", "g++"))
        self.assertIsNotNone(compiler, "A configured C++ compiler is required")
        environment = dict(os.environ, CXX=str(Path(compiler).resolve()))
        with tempfile.TemporaryDirectory(prefix="compiler-override-") as directory:
            temporary = Path(directory)
            source = temporary / "trap.cpp"
            source.write_text("int main() { return 93; }", encoding="utf-8")
            trap = temporary / ("g++.exe" if os.name == "nt" else "g++")
            subprocess.run([compiler, str(source), "-o", str(trap)], check=True)
            # Keep runtime-library paths intact (notably MinGW DLLs on Windows).
            environment["PATH"] = directory + os.pathsep + os.environ.get("PATH", "")
            self.assertEqual(Path(shutil.which("g++", path=environment["PATH"])), trap)
            result = subprocess.run(
                [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "NativeRunnerTests.test_failed_executable_stops_before_compiling_next_suite",
                    "NativeRunnerTests.test_success_from_other_directory_with_spaces_in_paths",
                ],
                env=environment,
                cwd=temporary,
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def run_runner(self, compiler_body, *, stale_binary=False):
        with tempfile.TemporaryDirectory(prefix="native-runner-") as directory:
            temporary = Path(directory)
            compiler = temporary / "fake compiler.py"
            compiler.write_text(compiler_body, encoding="utf-8")
            build = temporary / "build output"
            build.mkdir()
            suffix = ".exe" if os.name == "nt" else ""
            stale = build / f"ac_encoder_test{suffix}"
            if stale_binary:
                stale.write_text("old executable must not run", encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(RUNNER),
                    "--compiler",
                    sys.executable,
                    "--compiler-arg",
                    str(compiler),
                    "--build-dir",
                    str(build),
                ],
                cwd=temporary,
                capture_output=True,
                text=True,
                check=False,
            )
            return result, stale.exists()

    def test_compiler_failure_stops_before_running_a_stale_binary(self):
        result, stale_exists = self.run_runner(
            "import sys; print('compiler rejected source'); sys.exit(23)\n",
            stale_binary=True,
        )
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertIn("compiler rejected source", result.stdout)
        self.assertNotIn("== ac_state ==", result.stdout)
        self.assertFalse(stale_exists)

    def test_missing_compiler_has_actionable_error(self):
        result = subprocess.run(
            [sys.executable, str(RUNNER), "--compiler", "not-a-cxx-compiler-2710"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("compiler", result.stderr.lower())
        self.assertIn("--compiler", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_failed_executable_stops_before_compiling_next_suite(self):
        # Use the real compiler to build a tiny failing executable. This catches
        # a runner that checks only compilation, not test exit status.
        result, _ = self.run_runner(
            "import os, pathlib, subprocess, sys\n"
            "out = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
            "source = out.with_suffix('.cpp')\n"
            "source.write_text('int main() { return 17; }')\n"
            "subprocess.run([os.environ.get('CXX', 'g++'), str(source), '-o', str(out)], check=True)\n"
        )
        self.assertEqual(result.returncode, 17, result.stderr)
        self.assertNotIn("== ac_state ==", result.stdout)

    def test_success_from_other_directory_with_spaces_in_paths(self):
        # The real native suites run separately in check.py. Here tiny compiled
        # programs exercise orchestration, path quoting and successful sequencing.
        result, _ = self.run_runner(
            "import os, pathlib, subprocess, sys\n"
            "out = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
            "source = out.with_suffix('.cpp')\n"
            "source.write_text('int main() { return 0; }')\n"
            "subprocess.run([os.environ.get('CXX', 'g++'), str(source), '-o', str(out)], check=True)\n"
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("== ac_encoder ==", result.stdout)
        self.assertIn("== failsafe ==", result.stdout)
        self.assertIn("All 6 native suites passed.", result.stdout)


if __name__ == "__main__":
    unittest.main()
