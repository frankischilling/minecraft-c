"""Retained authored files for the native GameDirectory interface.

This driver runs only its supplied native test executable. The enclosing CTest
owner remains responsible for whole-tree supervision. No home lookup is replaced
and no original program or options/world input is used.
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


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def regular_pin(path: Path, maximum: int = 256 * 1024 * 1024) -> dict:
    information = path.lstat()
    require(stat.S_ISREG(information.st_mode) and not path.is_symlink()
            and not (getattr(information, "st_file_attributes", 0) & 0x400),
            f"regular non-reparse file required: {path}")
    require(information.st_size <= maximum, f"file bound: {path}")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while True:
            block = stream.read(65536)
            if not block:
                break
            size += len(block)
            require(size <= maximum, f"growing file bound: {path}")
            digest.update(block)
    require(size == information.st_size, f"file size changed during read: {path}")
    return {"bytes": size, "sha256": digest.hexdigest()}


def tree(root: Path) -> dict:
    result = {"directories": [], "files": {}}
    pending = [root]
    entries = 0
    while pending:
        directory = pending.pop()
        with os.scandir(directory) as listing:
            for entry in listing:
                entries += 1
                require(entries <= 64, "GameDirectory retained-tree entry bound")
                path = Path(entry.path)
                information = entry.stat(follow_symlinks=False)
                require(not entry.is_symlink()
                        and not (getattr(information, "st_file_attributes", 0) & 0x400),
                        f"unexpected link or reparse entry: {path}")
                relative = path.relative_to(root).as_posix()
                if stat.S_ISDIR(information.st_mode):
                    result["directories"].append(relative)
                    pending.append(path)
                else:
                    result["files"][relative] = regular_pin(path, STREAM_LIMIT)
    result["directories"].sort()
    result["files"] = dict(sorted(result["files"].items()))
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    require(work.is_dir() and not work.is_symlink(), "existing owned work directory required")
    if os.name != "nt":
        require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL,
                "waitable native child status required")
    before = regular_pin(binary)
    driver_before = regular_pin(Path(__file__).resolve(strict=True))
    parent = Path(tempfile.mkdtemp(prefix="mc-gd-", dir=work))
    root = parent / "authored-\u00e9-\U0001f680"
    receipt = {"schema": 1, "scope": "authored-native-GameDirectory-real-files",
               "root": str(root), "binary_before": before, "driver_before": driver_before,
               "passed": False, "forced_cleanup": False,
               "linux_default_selection": "not executed; native account policy is not overridden"}
    child = None
    cleanup_errors = []
    failure = None
    try:
        root.mkdir()
        (root / "existing").mkdir()
        sentinels = {"root.keep": b"root sentinel\x00\xff\r\n",
                     "existing/keep.bin": b"existing directory sentinel\n",
                     "regular-file": b"accepted regular file\x00unchanged\n",
                     "file-parent": b"blocking regular ancestor\n"}
        for relative, raw in sentinels.items():
            with (root / relative).open("xb") as stream:
                require(stream.write(raw) == len(raw), "authored sentinel write")
        expected_directories = {"existing", "nested", "nested/a", "nested/a/b", "nested/a/b/leaf",
                                "canonical-child", "canonical-child/leaf",
                                "unicode-\u00e9-\U0001f680", "unicode-\u00e9-\U0001f680/child"}
        environment = os.environ.copy()
        if os.name == "nt":
            appdata = root / "appdata"
            appdata.mkdir()
            # APPDATA is a real input to the child's native default route. The
            # native test also checks exact equality before requesting default.
            for key in tuple(environment):
                if key.casefold() == "appdata":
                    del environment[key]
            environment["APPDATA"] = str(appdata)
            expected_directories.update(("appdata", "appdata/.minecraft"))
            require(len(str(root / "missing/../canonical-child/leaf").encode("utf-16-le")) // 2
                    < 248, "owned fixture exceeds current Windows path boundary")
            receipt["windows_default_selection"] = {"APPDATA": str(appdata),
                                                       "confined": True}
        receipt["inputs_before"] = tree(root)
        command = [str(binary), str(root)]
        receipt["command"] = command
        with (parent / "native.stdout").open("xb") as stdout, \
                (parent / "native.stderr").open("xb") as stderr:
            child = subprocess.Popen(command, cwd=parent, env=environment,
                                     stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
            receipt["pid"] = child.pid
            deadline = time.monotonic() + 30
            while True:
                for name in ("native.stdout", "native.stderr"):
                    require((parent / name).stat().st_size <= STREAM_LIMIT,
                            "native diagnostic stream limit")
                status = child.poll()
                if status is not None:
                    receipt["returncode"] = child.wait(timeout=5)
                    require(receipt["returncode"] == status, "native reap disagreement")
                    break
                require(time.monotonic() < deadline, "native GameDirectory deadline")
                time.sleep(0.05)
        raw = (parent / "native.stdout").read_bytes()
        errors = (parent / "native.stderr").read_bytes()
        require(len(raw) <= STREAM_LIMIT and len(errors) <= STREAM_LIMIT,
                "final native diagnostic bounds")
        receipt["consumed_streams"] = {"native.stdout": {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()},
                                       "native.stderr": {"bytes": len(errors), "sha256": hashlib.sha256(errors).hexdigest()}}
        require(receipt["returncode"] == 0, "native interface test failed")
        require(errors == b"", "unexpected native stderr")
        match = re.fullmatch(rb"PASS: ([1-9][0-9]*) GameDirectory real-file interface checks\r?\n", raw)
        require(match is not None, "missing complete native test footer")
        receipt["native_checks"] = int(match.group(1))
        after = tree(root)
        require(set(after["directories"]) == expected_directories,
                "actual created directory family differs")
        require(after["files"] == receipt["inputs_before"]["files"],
                "sentinel file family or bytes changed")
        # These checks are independent from the component's reported prefix.
        for relative, expected in sentinels.items():
            require((root / relative).read_bytes() == expected, f"sentinel bytes: {relative}")
        require(not (root / "missing").exists(), "missing prefix was repaired")
        require(not (root / "file-parent/child").exists(), "file ancestor gained a child")
        receipt["independent_file_checks"] = ["exact directory family", "exact file family and pins",
                                               "four literal sentinel byte strings",
                                               "missing prefix remains absent",
                                               "regular ancestor remains blocking",
                                               "all created directories survive owner retirement"]
        receipt["outputs"] = after
        receipt["checked_parent_entries"] = sorted(path.name for path in parent.iterdir())
        require(set(receipt["checked_parent_entries"]) == {root.name, "native.stdout", "native.stderr"},
                "unexpected sibling outside the authored root")
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
            except BaseException as error:
                cleanup_errors.append(f"reap: {type(error).__name__}: {error}")
        for role, path, expected in (("binary_after", binary, before),
                                     ("driver_after", Path(__file__).resolve(), driver_before)):
            try:
                receipt[role] = regular_pin(path)
                require(receipt[role] == expected, f"{role} changed")
            except BaseException as error:
                cleanup_errors.append(f"{role}: {type(error).__name__}: {error}")
        for name in ("native.stdout", "native.stderr"):
            try:
                if (parent / name).exists():
                    # Retain the complete hash of a finite overshoot before
                    # rejecting it; the raw file is never discarded.
                    receipt[name] = regular_pin(parent / name)
                    require(receipt[name]["bytes"] <= STREAM_LIMIT, f"{name} exceeded limit")
                    consumed = receipt.get("consumed_streams", {})
                    if name in consumed:
                        require(receipt[name] == consumed[name], f"{name} changed after validation")
                    elif receipt["passed"]:
                        raise RuntimeError(f"missing consumed stream: {name}")
            except BaseException as error:
                cleanup_errors.append(f"{name}: {type(error).__name__}: {error}")
        try:
            receipt["retained_tree"] = tree(root) if root.is_dir() else None
            if receipt["passed"]:
                require(receipt["retained_tree"] == receipt["outputs"],
                        "retained output changed after independent checks")
        except BaseException as error:
            cleanup_errors.append(f"retained tree: {type(error).__name__}: {error}")
        try:
            receipt["retained_parent_entries_before_receipt"] = sorted(path.name for path in parent.iterdir())
            if receipt["passed"]:
                require(set(receipt["retained_parent_entries_before_receipt"]) ==
                        {root.name, "native.stdout", "native.stderr"},
                        "unexpected retained sibling outside the authored root")
        except BaseException as error:
            cleanup_errors.append(f"retained parent: {type(error).__name__}: {error}")
        if cleanup_errors:
            receipt["passed"] = False
            receipt["cleanup_errors"] = cleanup_errors
        with (parent / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
    if not receipt["passed"]:
        raise RuntimeError(f"GameDirectory failure retained at {parent}") from failure
    print(f"PASS: {receipt['native_checks']} native GameDirectory checks and retained real files")
    print("Capture: " + json.dumps(str(parent), ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
