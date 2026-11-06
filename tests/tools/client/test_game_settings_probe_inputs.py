"""Finite public-producer transport controls; no game-state oracle.

Run only after source review and integration. Each child is an actual public
probe executable supervised inside the enclosing CTest owner's process tree.
Fresh case directories and all consumed closed-pipe prefixes are retained.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


def pin(path: Path) -> dict:
    raw = path.read_bytes()
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def plan(initial: str = "absent\t-\t-", reload: str = "keep\t-\t-",
         constructor: str = "file") -> bytes:
    return ("MC_GAME_SETTINGS_INPUT_V1\ncase\tauthored-control\n" +
            f"constructor\t{constructor}\ncharset\tUTF-8\nlines\tLF\n" +
            f"initial\t{initial}\nreload\t{reload}\n").encode("ascii")


def rows(path: Path) -> list:
    raw = path.read_bytes()
    if len(raw) > 8388608 or not raw.endswith(b"\n") or b"\r" in raw:
        raise ValueError("complete bounded literal-LF native JSONL")
    return [json.loads(line.decode("ascii")) for line in raw.split(b"\n")[:-1]]


def bounded_first_byte(child: subprocess.Popen, retained: Path, timeout: float) -> None:
    """Persist each consumed increment before any later readiness/error path."""
    assert child.stdout is not None
    deadline = time.monotonic() + timeout
    with retained.open("xb", buffering=0) as stream:
        while time.monotonic() < deadline:
            if os.name == "nt":
                import ctypes
                import msvcrt
                from ctypes import wintypes
                peek = ctypes.WinDLL("kernel32", use_last_error=True).PeekNamedPipe
                peek.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                                 wintypes.LPVOID, ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
                peek.restype = wintypes.BOOL
                available = wintypes.DWORD()
                if not peek(msvcrt.get_osfhandle(child.stdout.fileno()), None, 0, None,
                            ctypes.byref(available), None):
                    raise OSError(ctypes.get_last_error(), "actual PeekNamedPipe")
                ready = available.value != 0
            else:
                import select
                ready = bool(select.select([child.stdout], [], [], 0.02)[0])
            if ready:
                data = os.read(child.stdout.fileno(), 1)
                if not data:
                    raise ValueError("actual pipe EOF before prefix")
                if stream.write(data) != len(data):
                    raise OSError("retained prefix write")
                # Only a transport byte is observed; no complete row is needed
                # for this intentionally blocked-large-header pipe refusal.
                return
            if child.poll() is not None:
                raise ValueError("child exited before closed-pipe stimulus")
            time.sleep(0.005)
    raise TimeoutError("bounded first native output byte")


def invoke(binary: Path, case: Path, content: bytes, expected: int,
           *, existing_output: bool = False, nonempty_scratch: bool = False,
           missing_plan: bool = False, pipe: bool = False) -> dict:
    case.mkdir()
    scratch = case / "scratch"
    scratch.mkdir()
    if nonempty_scratch:
        (scratch / "sentinel").write_bytes(b"existing scratch must survive")
    source = case / "input.plan"
    if not missing_plan:
        source.write_bytes(content)
    result = case / "observation.jsonl"
    if existing_output:
        result.write_bytes(b"existing output must survive\n")
    selected_before = {"binary": pin(binary)}
    if source.exists():
        selected_before["plan"] = pin(source)
    record = {"name": case.name, "expected_exit": expected, "before": selected_before,
              "passed": False, "forced_cleanup": False, "cleanup_errors": []}
    child = None
    failure = None
    try:
        with (case / "native.stderr").open("xb") as stderr:
            if pipe:
                child = subprocess.Popen([str(binary), str(source), str(scratch), "-"],
                                         stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                         stderr=stderr, bufsize=0)
                record["pid"] = child.pid
                bounded_first_byte(child, case / "consumed-prefix", 5)
                assert child.stdout is not None
                child.stdout.close()  # Actual owned reader closes; no OS signal injection.
                record["returncode"] = child.wait(timeout=5)
            else:
                with (case / "native.stdout").open("xb") as stdout:
                    child = subprocess.Popen([str(binary), str(source), str(scratch), str(result)],
                                             stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
                    record["pid"] = child.pid
                    record["returncode"] = child.wait(timeout=8)
        assert record["returncode"] == expected, "actual probe exit"
        assert (case / "native.stderr").stat().st_size <= 65536
        if not pipe:
            assert (case / "native.stdout").read_bytes() == b"", "file mode stdout"
        if existing_output:
            assert result.read_bytes() == b"existing output must survive\n"
            assert list(scratch.iterdir()) == [], "exclusive-output guard before source effects"
        elif nonempty_scratch:
            assert (scratch / "sentinel").read_bytes() == b"existing scratch must survive"
            assert not result.exists(), "nonempty root rejected before JSON output"
        elif expected == 2:
            assert not result.exists() and list(scratch.iterdir()) == [], "parser rejection side effects"
        elif pipe:
            assert (case / "consumed-prefix").read_bytes() == b"{"
            # Pipe capacity/scheduling may admit a source prefix. Every actual
            # artifact is retained; there is no claim that no call ran.
        else:
            actual = rows(result)
            header = actual[0]
            assert header["type"] == "header" and bytes.fromhex(header["plan_raw_hex"]) == content
            assert header["plan_bytes"] == len(content) and header["source_equivalence_qualified"] is False
            terminal = actual[-1]
            assert terminal["type"] == "terminal" and terminal["status"] == expected
            assert terminal["complete"] is (expected == 0)
            assert terminal["rows_before_terminal"] == len(actual) - 1
            if expected == 0:
                assert [row["type"] for row in actual] == [
                    "header", "input_control", "stage", "input_control", "stage", "stage",
                    "derived_input", "stage", "source_terminal", "native_retirement",
                    "native_retirement", "terminal"]
                stages = [row for row in actual if row["type"] == "stage"]
                assert [row["ordinal"] for row in stages] == [0, 1, 2, 3]
                assert [row["stage"] for row in stages] == ["constructor", "reload", "save", "fresh_reload"]
                for stage in stages:
                    assert stage["native_status"] == 0 and stage["native_error"]["status"] == 0
                    state = stage["state"]
                    assert state["instance_field_count"] == len(state["instance_fields"]) == 36
                    roles = [field["role"] for field in state["instance_fields"]]
                    assert len(set(roles)) == 36 and set(roles) == set("abcdefghijklmnopqrstuvwxyzABCDEFGHI") | {"N"}
                assert stages[0]["state"]["native_owner_alias"] == stages[1]["state"]["native_owner_alias"] == stages[2]["state"]["native_owner_alias"]
                assert stages[0]["state"]["native_owner_alias"] != stages[3]["state"]["native_owner_alias"]
                for row in actual:
                    if row["type"] == "input_control" and row["kind"] in ("raw", "default"):
                        assert row["actual_bytes_equal_declared_stimulus"] is True
                        assert row["after"]["hex"] == row["declared_stimulus_hex"]
                    if row["type"] == "native_retirement":
                        assert row["status"] == 0 and row["caller_slot_null"] is True
                assert actual[8]["stage_count"] == terminal["stage_count"] == 4
                assert terminal["native_retirement_count"] == 2
            else:
                assert terminal["stage_count"] == 0 and not any(row["type"] == "source_terminal" for row in actual)
                controls = [row for row in actual if row["type"] == "input_control"]
                assert len(controls) == 1 and controls[0]["actual_bytes_equal_declared_stimulus"] is False
                assert controls[0]["preparation_complete"] is False
                assert (scratch / "first" / "options.txt").read_bytes() == b"A"
                assert any(row["type"] == "failure" and "actual stimulus bytes" in row["reason"] for row in actual)
        record["passed"] = True
    except BaseException as error:
        failure = error
        record["failure"] = f"{type(error).__name__}: {error}"
    finally:
        # A generic wait/read/control exception must not skip any closure path.
        if child is not None:
            possibly_running = True
            try:
                possibly_running = child.poll() is None
            except BaseException as error:
                record["cleanup_errors"].append(f"query: {type(error).__name__}: {error}")
            if possibly_running:
                try:
                    child.kill()
                    record["forced_cleanup"] = True
                    record["passed"] = False
                except BaseException as error:
                    record["cleanup_errors"].append(f"kill: {type(error).__name__}: {error}")
            try:
                child.wait(timeout=5)
            except BaseException as error:
                record["cleanup_errors"].append(f"reap: {type(error).__name__}: {error}")
            try:
                if child.stdout is not None:
                    child.stdout.close()
            except BaseException as error:
                record["cleanup_errors"].append(f"pipe close: {type(error).__name__}: {error}")
        for name, path in (("binary", binary), ("plan", source)):
            if name not in selected_before:
                continue
            try:
                record.setdefault("after", {})[name] = pin(path)
                assert record["after"][name] == selected_before[name], "selected input changed"
            except BaseException as error:
                record["cleanup_errors"].append(f"{name} pin: {type(error).__name__}: {error}")
        record["outputs"] = {}
        try:
            inventory = sorted(case.rglob("*"))
        except BaseException as error:
            inventory = []
            record["cleanup_errors"].append(f"inventory: {type(error).__name__}: {error}")
        for path in inventory:
            try:
                if path.is_file():
                    record["outputs"][str(path.relative_to(case))] = pin(path)
            except BaseException as error:
                record["cleanup_errors"].append(f"artifact pin: {type(error).__name__}: {error}")
        if record["cleanup_errors"]:
            record["passed"] = False
        with (case / "receipt.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(record, stream, indent=2, sort_keys=True)
            stream.write("\n")
    if not record["passed"]:
        raise RuntimeError(f"native probe control failure retained at {case}") from failure
    return record


def main() -> int:
    if not __debug__:
        raise RuntimeError("probe controls require Python without -O")
    if os.name != "nt" and signal.getsignal(signal.SIGCHLD) != signal.SIG_DFL:
        raise RuntimeError("direct-child test waits require SIGCHLD default")
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.executable.resolve(strict=True)
    work = args.work_dir.resolve(strict=True)
    if not work.is_dir() or work.is_symlink():
        raise RuntimeError("probe controls require an existing owned work directory")
    parent = Path(tempfile.mkdtemp(prefix="mc-settings-probe-controls-", dir=work))
    root = parent / "authored-\u00e9-\U0001f680"
    root.mkdir()
    base = plan()
    cases = [("positive-absent", base, 0, {}),
             ("positive-noarg", plan(constructor="noarg"), 0, {}),
             ("positive-default", plan("default\t006d0075007300690063003a0030002e00320035000a\t6d757369633a302e32350a"), 0, {}),
             ("positive-raw", plan("raw\t-\t6d757369633a302e32350d0a"), 0, {}),
             ("positive-directory", plan("directory\t-\t-"), 0, {}),
             ("missing-plan", base, 2, {"missing_plan": True}),
             ("existing-output", base, 2, {"existing_output": True}),
             ("nonempty-scratch", base, 2, {"nonempty_scratch": True}),
             ("wrong-schema", base.replace(b"INPUT_V1", b"INPUT_V2"), 2, {}),
             ("no-final-LF", base[:-1], 2, {}),
             ("CRLF-plan", base.replace(b"\n", b"\r\n"), 2, {}),
             ("NUL-plan", base.replace(b"authored", b"auth\x00red"), 2, {}),
             ("extra-state-field", base + b"expected_music\t1.0\n", 2, {}),
             ("extra-token", base.replace(b"initial\tabsent\t-\t-", b"initial\tabsent\t-\t-\textra"), 2, {}),
             ("default-charset", base.replace(b"UTF-8", b"DEFAULT"), 2, {}),
             ("bad-lines", base.replace(b"lines\tLF", b"lines\tCR"), 2, {}),
             ("noarg-edit", plan("raw\t-\t41", constructor="noarg"), 2, {}),
             ("raw-odd-hex", plan("raw\t-\t0"), 2, {}),
             ("raw-uppercase", plan("raw\t-\tAF"), 2, {}),
             ("raw-text-field", plan("raw\t0041\t41"), 2, {}),
             ("lone-surrogate", plan("default\td800\t3f"), 2, {}),
             ("split-unit", plan("default\t041\t41"), 2, {}),
             ("overflow-input", plan("raw\t-\t" + "41"*65537), 2, {}),
             ("overflow-plan", b"X"*1048577, 2, {}),
             ("wrong-stimulus", plan("default\t0041\t42"), 1, {}),
             ("closed-pipe", plan("raw\t-\t" + "41"*65536), 1, {"pipe": True})]
    records = []
    for name, content, expected, options in cases:
        records.append(invoke(binary, root / name, content, expected, **options))
    report = {"schema": 1, "scope": "native GameSettings producer transport controls",
              "passed": all(record["passed"] for record in records), "cases": records,
              "original_or_JVM_execution": False, "game_state_equivalence_claimed": False}
    with (root / "report.json").open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print(f"PASS: {len(records)} GameSettings producer transport controls")
    print("Capture: " + json.dumps(str(root), ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
