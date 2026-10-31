"""Real files and independent byte checks for the public settings interface.

Fresh inputs/results are retained. This is a native test driver, not an
original-game producer or a claim about unobserved Java stream closure.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile


def pin(path: Path) -> dict:
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def main() -> int:
    if not __debug__:
        raise RuntimeError("GameSettings file assertions require Python without -O")
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    if not work.is_dir() or work.is_symlink():
        raise RuntimeError("GameSettings tests require an existing owned work directory")
    if os.name != "nt" and signal.getsignal(signal.SIGCHLD) != signal.SIG_DFL:
        raise RuntimeError("GameSettings tests require waitable child exit status")
    # The Unicode directory reaches actual wide-path adapters on Windows.
    parent = Path(tempfile.mkdtemp(prefix="mc-game-settings-", dir=work))
    root = parent / "authored-\u00e9-\U0001f680"
    root.mkdir()
    names = ("ordinary", "lexical", "aliases", "encoding", "utf8-output", "cp1252",
             "owned-directory", "raw-strings", "limits")
    for name in names:
        (root / name).mkdir()
    (root / "owned-directory" / "options.txt").mkdir()
    # absent-parent/child deliberately stays absent: no parent mkdir in save.
    before = pin(binary)
    receipt = {"schema": 1, "scope": "authored-native-GameSettings-real-files",
               "root": str(root), "binary_before": before, "passed": False}
    child = None
    cleanup_errors = []
    failure = None
    try:
        with (parent / "native.stdout").open("xb") as stdout, \
                (parent / "native.stderr").open("xb") as stderr:
            child = subprocess.Popen([str(binary), str(root)], stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr)
            receipt["pid"] = child.pid
            receipt["returncode"] = child.wait(timeout=45)
        raw = (parent / "native.stdout").read_bytes()
        errors = (parent / "native.stderr").read_bytes()
        assert len(raw) <= 65536 and len(errors) <= 65536, "native diagnostic bound"
        assert receipt["returncode"] == 0, "native test failed"
        assert errors == b"", "unexpected native stderr"
        match = re.fullmatch(rb"PASS: ([1-9][0-9]*) GameSettings persistence interface checks\r?\n", raw)
        assert match, "missing complete native footer"
        receipt["native_checks"] = int(match.group(1))

        prefix = (b"music:1.0\nsound:0.25\ninvertYMouse:false\nmouseSensitivity:0.5\n"
                  b"viewDistance:0\nguiScale:0\nbobView:true\nanaglyph3d:false\n"
                  b"advancedOpengl:false\nfpsLimit:1\ndifficulty:2\nfancyGraphics:true\nao:true\n")
        tail = (b"\nlastServer:null\nkey_key.forward:17\nkey_key.left:30\nkey_key.back:31\n"
                b"key_key.right:32\nkey_key.jump:57\nkey_key.sneak:42\nkey_key.drop:16\n"
                b"key_key.inventory:18\nkey_key.chat:20\nkey_key.fog:33\n")
        expected_utf8 = prefix + b"skin:C\xc3\xa9\xe2\x82\xac\xf0\x9f\x9a\x80?" + tail
        assert (root / "utf8-output" / "options.txt").read_bytes() == expected_utf8
        expected_cp = (prefix + b"skin:C\xe9\x80??" + tail).replace(b"\n", b"\r\n")
        assert (root / "cp1252" / "options.txt").read_bytes() == expected_cp
        assert (root / "owned-directory" / "options.txt").is_dir()
        assert not (root / "absent-parent").exists()
        partial = (root / "encoding" / "options.txt").read_bytes()
        assert len(partial) == 16384 and partial.startswith(prefix) and partial.endswith(b"P")
        raw_strings = (root / "raw-strings" / "options.txt").read_bytes()
        assert b"skin:A:B\nsound:0.5\x00Z\nlastServer:null\n" in raw_strings
        receipt["independent_file_checks"] = 6
        receipt["passed"] = True
    except BaseException as error:
        failure = error
        receipt["failure"] = f"{type(error).__name__}: {error}"
    finally:
        # Independent bounded attempts: an exceptional wait must not skip the
        # final kill/reap. The enclosing CTest's owner supervises its whole tree.
        if child is not None:
            try:
                if child.poll() is None:
                    child.kill()
                    receipt["forced_cleanup"] = True
                    receipt["passed"] = False
            except BaseException as error:
                cleanup_errors.append(f"kill: {type(error).__name__}: {error}")
            try:
                child.wait(timeout=5)
            except BaseException as error:
                cleanup_errors.append(f"reap: {type(error).__name__}: {error}")
        try:
            receipt["binary_after"] = pin(binary)
            assert receipt["binary_after"] == before, "tested binary changed"
        except BaseException as error:
            cleanup_errors.append(f"binary: {type(error).__name__}: {error}")
        for name in ("native.stdout", "native.stderr"):
            try:
                receipt[name] = pin(parent / name)
            except BaseException as error:
                cleanup_errors.append(f"{name}: {type(error).__name__}: {error}")
        if cleanup_errors:
            receipt["passed"] = False
            receipt["cleanup_errors"] = cleanup_errors
        receipt["outputs"] = {}
        try:
            paths = sorted(root.rglob("*"))
        except BaseException as error:
            paths = []
            cleanup_errors.append(f"output inventory: {type(error).__name__}: {error}")
        for path in paths:
            try:
                if path.is_file():
                    receipt["outputs"][str(path.relative_to(root))] = pin(path)
            except BaseException as error:
                cleanup_errors.append(f"{path}: {type(error).__name__}: {error}")
        if cleanup_errors:
            receipt["passed"] = False
            receipt["cleanup_errors"] = cleanup_errors
        with (parent / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
    if not receipt["passed"]:
        if failure is not None:
            raise RuntimeError(f"GameSettings file failure retained at {parent}") from failure
        raise RuntimeError(f"GameSettings cleanup failure retained at {parent}")
    print(f"PASS: {receipt['native_checks']} native and 6 independent GameSettings file checks")
    print("Capture: " + json.dumps(str(parent), ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
