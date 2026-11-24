"""Retain authored files and check the real GameSettings option actions.

Only the supplied native test executable is launched. The enclosing CTest
owner supervises its complete process tree. No original program is executed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import tempfile
import time


STREAM_LIMIT = 65536


def need(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def pin(path: Path, maximum: int = 256 * 1024 * 1024) -> dict:
    information = path.lstat()
    need(stat.S_ISREG(information.st_mode) and not path.is_symlink()
         and not (getattr(information, "st_file_attributes", 0) & 0x400),
         f"regular non-reparse file required: {path}")
    need(information.st_size <= maximum, f"file bound: {path}")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while True:
            block = stream.read(65536)
            if not block:
                break
            size += len(block)
            need(size <= maximum, f"growing file bound: {path}")
            digest.update(block)
    need(size == information.st_size, f"complete file read: {path}")
    return {"bytes": size, "sha256": digest.hexdigest()}


def tree(root: Path) -> dict:
    directories, files = [], {}
    pending, entries, total = [root], 0, 0
    while pending:
        current = pending.pop()
        with os.scandir(current) as listing:
            for item in listing:
                entries += 1
                need(entries <= 128, "retained file-family bound")
                path = Path(item.path)
                information = item.stat(follow_symlinks=False)
                need(not item.is_symlink()
                     and not (getattr(information, "st_file_attributes", 0) & 0x400),
                     f"unexpected link/reparse member: {path}")
                relative = path.relative_to(root).as_posix()
                if stat.S_ISDIR(information.st_mode):
                    directories.append(relative)
                    pending.append(path)
                else:
                    files[relative] = pin(path, 32768)
                    total += files[relative]["bytes"]
                    need(total <= 1024 * 1024, "retained file-byte bound")
    return {"directories": sorted(directories), "files": dict(sorted(files.items()))}


def write_exclusive(path: Path, raw: bytes) -> None:
    with path.open("xb") as stream:
        need(stream.write(raw) == len(raw), "authored input write")


def read_limited(path: Path, limit: int) -> bytes:
    before = pin(path, limit)
    with path.open("rb") as stream:
        raw = stream.read(limit + 1)
    consumed = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
    after = pin(path, limit)
    need(len(raw) <= limit and consumed == before == after, f"complete stable consumed file: {path}")
    return raw


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    need(work.is_dir() and not work.is_symlink(), "existing owned work directory required")
    if os.name != "nt":
        need(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, "waitable child status required")
    binary_before = pin(binary)
    driver = Path(__file__).resolve(strict=True)
    driver_before = pin(driver)
    parent = Path(tempfile.mkdtemp(prefix="mc-settings-actions-", dir=work))
    root = parent / "authored-\u00e9-\U0001f680"
    archive = parent / "inputs"
    receipt = {"schema": 1, "scope": "authored-native-GameSettings-option-action-real-files",
               "root": str(root), "passed": False, "forced_cleanup": False,
               "binary_before": binary_before, "driver_before": driver_before,
               "original_or_effect_owner_execution": False,
               "ownership": "direct Popen child; enclosing CTest owner supervises descendants"}
    child = None
    failure = None
    cleanup_errors = []
    checked = []
    consumed_files = {}
    receipt["consumed_files"] = consumed_files

    def consume(relative: str) -> bytes:
        raw = read_limited(root / relative, 32768)
        consumed_files[relative] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
        return raw

    try:
        root.mkdir()
        archive.mkdir()
        names = ("saved", "aliases", "late", "null-array", "no-save", "io-directory")
        for name in names:
            (root / name).mkdir()
        (root / "io-directory/options.txt").mkdir()
        no_save = b"authoredUnknown:preserve:every:byte\r\n"
        initial = {name + "/options.txt": b"authoredUnknown:initial\n"
                   for name in ("saved", "aliases", "late", "null-array")}
        initial["no-save/options.txt"] = no_save
        initial["io-directory/options.txt/retained.bin"] = b"directory sentinel\x00\xff"
        initial["keep.bin"] = b"root sentinel\x00\r\n\xff"
        for relative, raw in initial.items():
            write_exclusive(root / relative, raw)
        receipt["inputs_before"] = tree(root)
        for directory in receipt["inputs_before"]["directories"]:
            (archive / directory).mkdir(parents=True, exist_ok=True)
        for relative in receipt["inputs_before"]["files"]:
            write_exclusive(archive / relative, read_limited(root / relative, 32768))
        receipt["input_archive_before"] = tree(archive)
        need(receipt["input_archive_before"] == receipt["inputs_before"], "complete input archive join")
        command = [str(binary), str(root)]
        receipt["command"] = command
        with (parent / "native.stdout").open("xb") as stdout, \
                (parent / "native.stderr").open("xb") as stderr:
            child = subprocess.Popen(command, cwd=parent, stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr)
            receipt["pid"] = child.pid
            deadline = time.monotonic() + 45
            while True:
                for name in ("native.stdout", "native.stderr"):
                    need((parent / name).stat().st_size <= STREAM_LIMIT, "native stream bound")
                status = child.poll()
                if status is not None:
                    receipt["returncode"] = child.wait(timeout=5)
                    need(receipt["returncode"] == status, "native reap agreement")
                    break
                need(time.monotonic() < deadline, "native action deadline")
                time.sleep(0.05)
        receipt["closed_streams"] = stdout.closed and stderr.closed
        need(receipt["closed_streams"], "observer stream handles closed")
        raw = read_limited(parent / "native.stdout", STREAM_LIMIT)
        errors = read_limited(parent / "native.stderr", STREAM_LIMIT)
        receipt["consumed_streams"] = {
            "native.stdout": {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()},
            "native.stderr": {"bytes": len(errors), "sha256": hashlib.sha256(errors).hexdigest()},
        }
        need(len(raw) <= STREAM_LIMIT and len(errors) <= STREAM_LIMIT, "final stream bounds")
        need(receipt["returncode"] == 0 and errors == b"", "native action test status/stderr")
        match = re.fullmatch(rb"PASS: ([1-9][0-9]*) GameSettings option action interface checks\r?\n", raw)
        need(match is not None, "complete native action footer")
        receipt["native_checks"] = int(match.group(1))

        # Handwritten complete bytes, independent of the C save/parse routines.
        binding_tail = (b"key_key.forward:17\nkey_key.left:30\nkey_key.back:31\nkey_key.right:32\n"
                        b"key_key.jump:57\nkey_key.sneak:42\nkey_key.drop:16\n"
                        b"key_key.inventory:18\nkey_key.chat:20\nkey_key.fog:33\n")
        saved = (b"music:1.0\nsound:1.0\ninvertYMouse:true\nmouseSensitivity:-0.0\n"
                 b"viewDistance:3\nguiScale:2\nbobView:false\nanaglyph3d:false\nadvancedOpengl:false\n"
                 b"fpsLimit:-2\ndifficulty:3\nfancyGraphics:true\nao:true\nskin:Default\nlastServer:\n" +
                 binding_tail)
        need(consume("saved/options.txt") == saved, "literal full saved file")
        checked.append("literal full saved file after valid step0 and NULL-option resaves")
        defaults = (b"music:1.0\nsound:1.0\ninvertYMouse:false\nmouseSensitivity:0.5\n"
                    b"viewDistance:0\nguiScale:0\nbobView:true\nanaglyph3d:false\nadvancedOpengl:false\n"
                    b"fpsLimit:1\ndifficulty:2\nfancyGraphics:true\nao:true\n")
        aliases = (defaults + b"skin:Pack-\xc3\xa9\nlastServer:Pack-\xc3\xa9\n"
                   b"key_key.alias-\xc3\xa9:-2147483648\nkey_key.alias-\xc3\xa9:-2147483648\n")
        need(consume("aliases/options.txt") == aliases, "literal current-array alias file")
        checked.append("literal alias/UTF8 file with two current references and absent named left")
        late_prefix = (b"music:1.0\nsound:1.0\ninvertYMouse:true\nmouseSensitivity:0.5\n"
                       b"viewDistance:0\nguiScale:0\nbobView:true\nanaglyph3d:false\nadvancedOpengl:false\n"
                       b"fpsLimit:1\ndifficulty:2\nfancyGraphics:true\nao:true\nskin:")
        need(consume("late/options.txt") == (late_prefix + b"P" * 9000)[:8192],
             "literal committed prefix before late null binding")
        checked.append("exact 8192-byte committed partial file")
        need(consume("null-array/options.txt") == b"", "truncate without implicit flush")
        checked.append("empty actual file after caught null current array")
        need(consume("no-save/options.txt") == no_save, "float and missing-effect preservation")
        checked.append("exact no-save sentinel for float/no-op/unimplemented effects")
        need((root / "io-directory/options.txt").is_dir() and
             consume("io-directory/options.txt/retained.bin") ==
             initial["io-directory/options.txt/retained.bin"], "caught I/O directory and child")
        checked.append("directory kind and child survive caught save failure")
        need(not (root / "absent-parent").exists(), "save created a missing parent")
        checked.append("missing parent remains absent")
        after = tree(root)
        need(after["directories"] == receipt["inputs_before"]["directories"] and
             set(after["files"]) == set(initial) and
             consume("keep.bin") == initial["keep.bin"], "exact retained family and root sentinel")
        need(consumed_files == after["files"], "every literal-checked file joins the complete output inventory")
        receipt["parent_entries_before_receipt"] = sorted(item.name for item in parent.iterdir())
        need(set(receipt["parent_entries_before_receipt"]) ==
             {root.name, archive.name, "native.stdout", "native.stderr"}, "unexpected cwd file or directory")
        checked.append("exact whole file/directory family and confined cwd")
        receipt["outputs"] = after
        receipt["independent_file_checks"] = checked
        receipt["passed"] = True
    except BaseException as error:
        failure = error
        receipt["failure"] = f"{type(error).__name__}: {error}"
    finally:
        if child is not None:
            needs_kill = True
            try:
                needs_kill = child.poll() is None
            except BaseException as error:
                cleanup_errors.append(f"poll: {type(error).__name__}: {error}")
            if needs_kill:
                try:
                    child.kill()
                    receipt["forced_cleanup"] = True
                    receipt["passed"] = False
                except BaseException as error:
                    cleanup_errors.append(f"kill: {type(error).__name__}: {error}")
            try:
                receipt["final_returncode"] = child.wait(timeout=5)
                receipt["reaped"] = True
            except BaseException as error:
                cleanup_errors.append(f"reap: {type(error).__name__}: {error}")
        for role, path, expected in (("binary_after", binary, binary_before),
                                     ("driver_after", driver, driver_before)):
            try:
                receipt[role] = pin(path)
                need(receipt[role] == expected, f"unchanged {role}")
            except BaseException as error:
                cleanup_errors.append(f"{role}: {type(error).__name__}: {error}")
        for name in ("native.stdout", "native.stderr"):
            try:
                if (parent / name).exists():
                    receipt[name] = pin(parent / name)
                    need(receipt[name]["bytes"] <= STREAM_LIMIT, f"{name} exceeded limit")
                    if name in receipt.get("consumed_streams", {}):
                        need(receipt[name] == receipt["consumed_streams"][name], f"{name} changed after consumption")
            except BaseException as error:
                cleanup_errors.append(f"{name}: {type(error).__name__}: {error}")
        for role, directory in (("retained_tree", root), ("input_archive_after", archive)):
            try:
                receipt[role] = tree(directory) if directory.is_dir() else None
                if role == "input_archive_after":
                    need(receipt[role] == receipt.get("input_archive_before"), "input archive changed")
                elif receipt["passed"]:
                    need(receipt[role] == receipt["outputs"], "retained output changed after checks")
            except BaseException as error:
                cleanup_errors.append(f"{role}: {type(error).__name__}: {error}")
        if cleanup_errors:
            receipt["passed"] = False
            receipt["cleanup_errors"] = cleanup_errors
        with (parent / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
    if not receipt["passed"]:
        raise RuntimeError(f"GameSettings action failure retained at {parent}") from failure
    print(f"PASS: {receipt['native_checks']} native and {len(checked)} independent GameSettings action file checks")
    print("Capture: " + json.dumps(str(parent), ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
