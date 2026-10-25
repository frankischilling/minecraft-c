"""Retain authored sound files and check the native URL/MUS file owner.

The enclosing CTest owner supervises descendants. No original assets, JVM,
decoder or audio device are used. Inputs and complete native streams remain
under the selected work directory, with their consumed-byte hashes.
"""

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


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def consume(path, maximum):
    before = path.lstat()
    need(stat.S_ISREG(before.st_mode) and not path.is_symlink()
         and not getattr(before, "st_file_attributes", 0) & 0x400, "regular input")
    need(before.st_size <= maximum, "input bound")
    with path.open("rb") as stream:
        raw = stream.read(maximum + 1)
    after = path.lstat()
    need(len(raw) <= maximum and len(raw) == before.st_size
         and (before.st_ino, before.st_size, before.st_mtime_ns)
         == (after.st_ino, after.st_size, after.st_mtime_ns), "stable bounded input")
    return raw, {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    _, binary_before = consume(binary, 256 * 1024 * 1024)
    driver = Path(__file__).resolve(strict=True)
    _, driver_before = consume(driver, 65536)
    parent = Path(tempfile.mkdtemp(prefix="mc-sound-file-", dir=work))
    root = parent / "inputs"
    root.mkdir()
    # Authored literal vectors. The encrypted bytes use the Java String hash
    # of "stream%20sample.mus". They are not copied from a game resource.
    inputs = {"plain.bin": bytes.fromhex("00017f80ff4f67675300022a00ff8040"),
              "stream sample.mus": bytes.fromhex("c36776d7bba9e94ba2a4db75ff20e2d3")}
    for name, raw in inputs.items():
        with (root / name).open("xb") as stream:
            need(stream.write(raw) == len(raw), "complete authored file")
    before = {name: consume(root / name, 32)[1] for name in inputs}
    receipt = {"schema": 1, "scope": "authored-native-sound-file-URL-MUS",
               "root": str(root), "passed": False, "inputs_before": before,
               "binary_before": binary_before, "driver_before": driver_before,
               "original_or_decoder_or_device_execution": False,
               "forced_cleanup": False}
    child = None
    failure = None
    cleanup_errors = []
    try:
        command = [str(binary), str(root)]
        receipt["command"] = command
        with (parent / "native.stdout").open("xb") as stdout, \
                (parent / "native.stderr").open("xb") as stderr:
            child = subprocess.Popen(command, cwd=parent, stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr)
            receipt["pid"] = child.pid
            deadline = time.monotonic() + 30
            while True:
                need(all((parent / name).stat().st_size <= 65536
                         for name in ("native.stdout", "native.stderr")), "native stream bound")
                status = child.poll()
                if status is not None:
                    receipt["returncode"] = child.wait(timeout=5)
                    need(status == receipt["returncode"], "native reap agreement")
                    break
                need(time.monotonic() < deadline, "native deadline")
                time.sleep(0.02)
        receipt["closed_streams"] = stdout.closed and stderr.closed
        need(receipt["closed_streams"], "closed observation streams")
        raw, out_pin = consume(parent / "native.stdout", 65536)
        errors, err_pin = consume(parent / "native.stderr", 65536)
        receipt["consumed_streams"] = {"native.stdout": out_pin, "native.stderr": err_pin}
        need(receipt["returncode"] == 0 and errors == b"", "native result")
        match = re.fullmatch(rb"PASS: ([1-9][0-9]*) sound File interface checks\r?\n", raw)
        need(match is not None, "complete native footer")
        receipt["native_checks"] = int(match.group(1))
        receipt["inputs_after"] = {name: consume(root / name, 32)[1] for name in inputs}
        need(receipt["inputs_after"] == before, "unchanged complete inputs")
        need(set(p.name for p in root.iterdir()) == set(inputs), "unchanged input membership")
        for name, expected in inputs.items():
            need(consume(root / name, 32)[0] == expected, "literal retained input bytes")
        _, receipt["binary_after"] = consume(binary, 256 * 1024 * 1024)
        _, receipt["driver_after"] = consume(driver, 65536)
        need(receipt["binary_after"] == binary_before
             and receipt["driver_after"] == driver_before, "unchanged executable/driver")
        receipt["passed"] = True
    except BaseException as error:
        failure = f"{type(error).__name__}: {error}"
    finally:
        if child is not None:
            needs_kill = True
            try:
                needs_kill = child.poll() is None
            except BaseException as error:
                cleanup_errors.append(f"poll: {type(error).__name__}: {error}")
            if needs_kill:
                receipt["forced_cleanup"] = True
                try:
                    if os.name == "nt":
                        killer = subprocess.run(
                            [str(Path(os.environ["SystemRoot"]) / "System32/taskkill.exe"),
                             "/PID", str(child.pid), "/T", "/F"],
                            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL, timeout=5, check=False)
                        receipt["taskkill_returncode"] = killer.returncode
                    else:
                        child.send_signal(signal.SIGKILL)
                except BaseException as error:
                    cleanup_errors.append(f"kill: {type(error).__name__}: {error}")
            try:
                receipt["final_returncode"] = child.wait(timeout=5)
                receipt["reaped"] = True
            except BaseException as error:
                cleanup_errors.append(f"reap: {type(error).__name__}: {error}")
        for role, selected, expected, maximum in (
                ("binary_after", binary, binary_before, 256 * 1024 * 1024),
                ("driver_after", driver, driver_before, 65536)):
            try:
                _, receipt[role] = consume(selected, maximum)
                need(receipt[role] == expected, f"unchanged {role}")
            except BaseException as error:
                cleanup_errors.append(f"{role}: {type(error).__name__}: {error}")
        for name in ("native.stdout", "native.stderr"):
            try:
                if (parent / name).exists():
                    _, final = consume(parent / name, 65536)
                    receipt[name] = final
                    if name in receipt.get("consumed_streams", {}):
                        need(final == receipt["consumed_streams"][name], "retained stream bytes")
            except BaseException as error:
                cleanup_errors.append(f"{name}: {type(error).__name__}: {error}")
        try:
            receipt["retained_inputs"] = {name: consume(root / name, 32)[1] for name in inputs}
            need(receipt["retained_inputs"] == before, "retained literal input family")
            need(set(p.name for p in root.iterdir()) == set(inputs), "retained input membership")
        except BaseException as error:
            cleanup_errors.append(f"inputs: {type(error).__name__}: {error}")
        if failure is not None:
            receipt["failure"] = failure
        if cleanup_errors:
            receipt["cleanup_errors"] = cleanup_errors
        receipt["passed"] = (receipt["passed"] and failure is None and not cleanup_errors
                             and not receipt["forced_cleanup"] and receipt.get("reaped") is True
                             and receipt.get("final_returncode") == receipt.get("returncode") == 0)
        with (parent / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
    if not receipt["passed"]:
        raise RuntimeError(f"sound File failure retained at {parent}: {failure or cleanup_errors}")
    print(f"PASS: {receipt['native_checks']} native checks; 2 literal sound input files preserved")
    print("Capture: " + json.dumps(str(parent), ensure_ascii=True))


if __name__ == "__main__":
    main()
