"""Generate authored nonzero Vorbis inputs and check the production decoder.

Both native children are supervised by the enclosing CTest owner. Encoded
fixtures are outputs of the first child and pinned inputs to the second; no
original assets, expected-state input, device or JVM is used.
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


STREAM_LIMIT = 65536
FILE_LIMIT = 2097152
TOTAL_LIMIT = 6291456
BINARY_LIMIT = 268435456
INPUTS = {"mono32k.ogg", "stereo48k.ogg", "chain.ogg", "mono44k-long.ogg"}
OUTPUTS = {"mono32k.pcm": 8192, "stereo48k.pcm": 19200,
           "chain.pcm": 27392, "mono44k-long.pcm": 160000}


def need(condition, message):
    if not condition:
        raise RuntimeError(message)


def consume(path, maximum):
    before = path.lstat()
    need(stat.S_ISREG(before.st_mode) and not path.is_symlink()
         and not getattr(before, "st_file_attributes", 0) & 0x400, "regular non-reparse file")
    need(before.st_size <= maximum, "pre-read file bound")
    with path.open("rb") as stream:
        raw = stream.read(maximum + 1)
    after = path.lstat()
    need(len(raw) <= maximum and len(raw) == before.st_size
         and (before.st_ino, before.st_size, before.st_mtime_ns)
         == (after.st_ino, after.st_size, after.st_mtime_ns), "stable complete file read")
    return raw, {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def inventory(root):
    information = root.lstat()
    need(stat.S_ISDIR(information.st_mode) and not root.is_symlink()
         and not getattr(information, "st_file_attributes", 0) & 0x400, "retained regular directory")
    members = list(root.iterdir())
    need(len(members) <= 8, "retained member bound")
    result = {path.name: consume(path, FILE_LIMIT)[1] for path in sorted(members)}
    need(sum(value["bytes"] for value in result.values()) <= TOTAL_LIMIT, "retained byte bound")
    return result


def live_bounds(root):
    information = root.lstat()
    need(stat.S_ISDIR(information.st_mode) and not root.is_symlink()
         and not getattr(information, "st_file_attributes", 0) & 0x400, "live owned fixture directory")
    members = list(root.iterdir())
    need(len(members) <= 8, "live fixture member bound")
    total = 0
    for path in members:
        information = path.lstat()
        need(stat.S_ISREG(information.st_mode) and not path.is_symlink()
             and not getattr(information, "st_file_attributes", 0) & 0x400,
             "live regular fixture file")
        need(information.st_size <= FILE_LIMIT, "live fixture file bound")
        total += information.st_size
    need(total <= TOTAL_LIMIT, "live aggregate fixture bound")


def run_phase(binary, parent, root, name, mode, pattern, receipt):
    row = {"name": name, "command": [str(binary), mode, str(root)], "passed": False,
           "forced_cleanup": False,
           "ownership": "direct Popen child; enclosing CTest owner supervises descendants"}
    receipt["phases"].append(row)
    stdout_path, stderr_path = parent / (name + ".stdout"), parent / (name + ".stderr")
    row["stdout_path"], row["stderr_path"] = stdout_path.name, stderr_path.name
    child = None
    failure = None
    cleanup_errors = []
    stdout = stderr = None
    start = time.monotonic()
    try:
        with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
            child = subprocess.Popen(row["command"], cwd=parent, stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr)
            row["pid"] = child.pid
            deadline = time.monotonic() + 45
            while True:
                live_bounds(root)
                need(stdout_path.stat().st_size <= STREAM_LIMIT
                     and stderr_path.stat().st_size <= STREAM_LIMIT, "native stream bound")
                status = child.poll()
                if status is not None:
                    row["returncode"] = child.wait(timeout=5)
                    need(row["returncode"] == status, "native reap agreement")
                    break
                need(time.monotonic() < deadline, "native phase deadline")
                time.sleep(0.02)
        row["closed_streams"] = stdout.closed and stderr.closed
        need(row["closed_streams"], "closed native observation handles")
        raw, out_pin = consume(stdout_path, STREAM_LIMIT)
        errors, err_pin = consume(stderr_path, STREAM_LIMIT)
        row["consumed_streams"] = {"stdout": out_pin, "stderr": err_pin}
        need(row["returncode"] == 0 and errors == b"",
             f"native phase status={row['returncode']}; stderr={errors!r}")
        match = re.fullmatch(pattern, raw)
        need(match is not None, "complete native phase footer")
        if match.lastindex:
            row["native_checks"] = int(match.group(1))
        row["passed"] = True
    except BaseException as error:
        failure = error
        row["failure"] = f"{type(error).__name__}: {error}"
    finally:
        row["closed_streams"] = ((stdout is None or stdout.closed)
                                 and (stderr is None or stderr.closed))
        if child is not None:
            needs_kill = True
            try:
                needs_kill = child.poll() is None
            except BaseException as error:
                cleanup_errors.append(f"poll: {type(error).__name__}: {error}")
            if needs_kill:
                row["forced_cleanup"] = True
                try:
                    if os.name == "nt":
                        killer = subprocess.run(
                            [str(Path(os.environ["SystemRoot"]) / "System32/taskkill.exe"),
                             "/PID", str(child.pid), "/T", "/F"],
                            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL, timeout=5, check=False)
                        row["taskkill_returncode"] = killer.returncode
                    else:
                        child.send_signal(signal.SIGKILL)
                except BaseException as error:
                    cleanup_errors.append(f"kill: {type(error).__name__}: {error}")
            try:
                row["final_returncode"] = child.wait(timeout=5)
                row["reaped"] = True
            except BaseException as error:
                cleanup_errors.append(f"reap: {type(error).__name__}: {error}")
        for key, path in (("stdout", stdout_path), ("stderr", stderr_path)):
            try:
                if path.exists():
                    _, row[key] = consume(path, STREAM_LIMIT)
                    if key in row.get("consumed_streams", {}):
                        need(row[key] == row["consumed_streams"][key], "retained consumed stream")
            except BaseException as error:
                cleanup_errors.append(f"{key}: {type(error).__name__}: {error}")
        row["seconds"] = time.monotonic() - start
        if cleanup_errors:
            row["cleanup_errors"] = cleanup_errors
        row["passed"] = (row["passed"] and failure is None and not cleanup_errors
                         and not row["forced_cleanup"] and row.get("reaped") is True
                         and row.get("returncode") == row.get("final_returncode") == 0)
    need(row["passed"], f"native {name} failed: {row.get('failure', cleanup_errors)}")
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    need(work.is_dir() and not work.is_symlink(), "existing owned work directory")
    if os.name != "nt":
        need(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, "waitable child status")
    _, binary_before = consume(binary, BINARY_LIMIT)
    driver = Path(__file__).resolve(strict=True)
    _, driver_before = consume(driver, 128 * 1024)
    parent = Path(tempfile.mkdtemp(prefix="mc-vorbis-nonzero-", dir=work))
    root = parent / "authored-\u00e9-\U0001f680"
    root.mkdir()
    receipt = {"schema": 1, "scope": "authored-native-nonzero-Vorbis-codec",
               "root": str(root), "passed": False, "phases": [],
               "binary_before": binary_before, "driver_before": driver_before,
               "original_or_device_execution": False,
               "fixture_source": "native libvorbisenc from exact authored nonzero rational PCM; no original assets"}
    failure = None
    cleanup_errors = []
    before = None
    try:
        run_phase(binary, parent, root, "create", "--create-fixtures",
                  rb"PASS: 4 authored nonzero Vorbis fixtures\r?\n", receipt)
        before = receipt["inputs_before"] = inventory(root)
        need(set(before) == INPUTS, "complete generated input family before check child")
        encoded = {}
        consumed_inputs = {}
        for name in sorted(INPUTS):
            encoded[name], consumed_inputs[name] = consume(root / name, FILE_LIMIT)
        need(consumed_inputs == before, "complete consumed encoded inputs")
        need(encoded["chain.ogg"] == encoded["mono32k.ogg"] + encoded["stereo48k.ogg"],
             "encoded chain is the exact two authored streams")
        receipt["consumed_inputs"] = consumed_inputs
        row = run_phase(binary, parent, root, "check", "--check",
                        rb"PASS: ([1-9][0-9]*) nonzero Vorbis codec checks\r?\n", receipt)
        receipt["native_checks"] = row["native_checks"]
        after = inventory(root)
        need(set(after) == INPUTS | set(OUTPUTS), "exact inputs and PCM output family")
        receipt["inputs_after"] = {name: after[name] for name in sorted(INPUTS)}
        need(receipt["inputs_after"] == before, "unchanged generated inputs after check")
        pcm = {}
        consumed = {}
        for name, length in OUTPUTS.items():
            pcm[name], consumed[name] = consume(root / name, FILE_LIMIT)
            need(len(pcm[name]) == length and any(pcm[name]),
                 "exact authored nonzero-PCM length/content: " + name)
        need(consumed == {name: after[name] for name in OUTPUTS}, "consumed PCM pins join output inventory")
        need(pcm["chain.pcm"] == pcm["mono32k.pcm"] + pcm["stereo48k.pcm"],
             "both chained format PCM prefixes")
        stereo = pcm["stereo48k.pcm"]
        need(any(stereo[i] or stereo[i + 1] for i in range(0, len(stereo), 4))
             and any(stereo[i + 2] or stereo[i + 3] for i in range(0, len(stereo), 4))
             and any(stereo[i:i + 2] != stereo[i + 2:i + 4] for i in range(0, len(stereo), 4)),
             "both stereo planes remain nonzero and distinct")
        receipt["consumed_outputs"] = consumed
        receipt["outputs"] = {name: after[name] for name in sorted(OUTPUTS)}
        receipt["checked_tree"] = after
        receipt["passed"] = True
    except BaseException as error:
        failure = error
        receipt["failure"] = f"{type(error).__name__}: {error}"
    finally:
        for role, path, expected, maximum in (
                ("binary_after", binary, binary_before, BINARY_LIMIT),
                ("driver_after", driver, driver_before, 128 * 1024)):
            try:
                _, receipt[role] = consume(path, maximum)
                need(receipt[role] == expected, "unchanged " + role)
            except BaseException as error:
                cleanup_errors.append(f"{role}: {type(error).__name__}: {error}")
        try:
            retained = receipt["retained_tree"] = inventory(root)
            if before is not None:
                need({name: retained[name] for name in INPUTS} == before, "retained complete input pins")
            if receipt["passed"]:
                need(retained == receipt["checked_tree"], "retained complete checked tree")
                need(receipt["consumed_outputs"] == receipt["outputs"], "retained asserted PCM pins")
        except BaseException as error:
            cleanup_errors.append(f"retained files: {type(error).__name__}: {error}")
        expected_parent = {root.name}
        for row in receipt["phases"]:
            for key in ("stdout", "stderr"):
                path = parent / row[key + "_path"]
                expected_parent.add(path.name)
                try:
                    if path.exists():
                        _, final = consume(path, STREAM_LIMIT)
                        need(final == row[key], "final independent native stream pin")
                except BaseException as error:
                    cleanup_errors.append(f"{row['name']}/{key}: {type(error).__name__}: {error}")
        try:
            receipt["parent_entries_before_receipt"] = sorted(path.name for path in parent.iterdir())
            need(set(receipt["parent_entries_before_receipt"]) == expected_parent,
                 "exact retained parent family")
        except BaseException as error:
            cleanup_errors.append(f"retained parent: {type(error).__name__}: {error}")
        if cleanup_errors:
            receipt["cleanup_errors"] = cleanup_errors
        receipt["passed"] = (receipt["passed"] and failure is None and not cleanup_errors
                             and len(receipt["phases"]) == 2
                             and all(row["passed"] for row in receipt["phases"]))
        with (parent / "receipt.json").open("x", encoding="utf-8", newline="\n") as output:
            json.dump(receipt, output, indent=2, sort_keys=True)
            output.write("\n")
    if not receipt["passed"]:
        raise RuntimeError(f"Nonzero Vorbis failure retained at {parent}") from failure
    print(f"PASS: {receipt['native_checks']} native nonzero Vorbis checks; 4 authored inputs and 4 PCM outputs preserved")
    print("Capture: " + json.dumps(str(parent), ensure_ascii=True))


if __name__ == "__main__":
    main()
