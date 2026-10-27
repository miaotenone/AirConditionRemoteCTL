"""A formatting violation must fail the same entrypoint CI uses."""

import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class TextPolicyTests(unittest.TestCase):
    def check_text(self, content):
        checker = ROOT / "scripts" / "check.py"
        self.assertTrue(checker.is_file(), "quality gate has not been implemented")
        spec = importlib.util.spec_from_file_location("quality_gate", checker)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.cpp"
            path.write_bytes(content)
            return module.text_errors([path])

    def test_lf_utf8_text_passes(self):
        self.assertEqual(self.check_text("// 中文\n".encode()), [])

    def test_crlf_is_allowed_for_windows_worktrees(self):
        # Git enforces LF on commit; a fresh Windows tree may predate attributes.
        self.assertEqual(self.check_text(b"int value;\r\n"), [])

    def test_missing_final_newline_fails(self):
        self.assertTrue(self.check_text(b"int value;"))

    def test_trailing_whitespace_fails(self):
        self.assertTrue(self.check_text(b"int value; \n"))

    def test_invalid_utf8_fails(self):
        self.assertTrue(self.check_text(b"\xff\n"))

    def test_utf8_bom_fails(self):
        self.assertTrue(self.check_text(b"\xef\xbb\xbfint value;\n"))


class QualityCommandTests(unittest.TestCase):
    def test_fix_repairs_format_before_linting_legacy_one_line_statement(self):
        # Catch the real ordering bug: Ruff check cannot auto-fix E701, while
        # Ruff format can. --fix must complete in one invocation, not require
        # manually invoking the formatter before retrying the quality command.
        with tempfile.TemporaryDirectory(prefix="quality-gate-") as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "src").mkdir()
            (root / "tests" / "python").mkdir(parents=True)
            for name in (
                ".clang-format",
                "pyproject.toml",
                "requirements-dev.txt",
                "platformio.ini",
            ):
                shutil.copyfile(ROOT / name, root / name)
            shutil.copyfile(ROOT / "scripts" / "check.py", root / "scripts" / "check.py")
            sample = root / "scripts" / "sample.py"
            sample.write_text("if True: print('hello')\n", encoding="utf-8")
            (root / "src" / "sample.cpp").write_text("int value=1;\n", encoding="utf-8")
            command = [sys.executable, str(root / "scripts" / "check.py"), "--format-only"]
            before = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertNotEqual(before.returncode, 0, before.stdout)
            fixed = subprocess.run([*command, "--fix"], capture_output=True, text=True, check=False)
            self.assertEqual(fixed.returncode, 0, fixed.stdout + fixed.stderr)
            after = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(after.returncode, 0, after.stdout + after.stderr)


if __name__ == "__main__":
    unittest.main()
