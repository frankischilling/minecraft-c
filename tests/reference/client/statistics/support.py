"""Bounded reference-driver utilities; no runtime stats implementation."""

import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import threading
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def fresh_directory(work_dir, prefix="run-"):
    work_dir.mkdir(parents=True, exist_ok=True)
    return Path(tempfile.mkdtemp(prefix=prefix, dir=work_dir.resolve()))


def run_bounded(command, cwd, prefix, timeout=120, limit=2097152, env=None):
    """Adapt the reviewed environment capture, preserving CompletedProcess API."""
    cwd = Path(cwd)
    exceeded = threading.Event()
    counts = {"stdout": 0, "stderr": 0}
    reader_errors = {}
    error_lock = threading.Lock()
    process = None
    code = None
    workers = []

    def record(name, phase, error):
        with error_lock:
            reader_errors.setdefault(name, []).append({"phase": phase,
                "type": type(error).__name__, "message": str(error)})

    def stop():
        if process is not None:
            try:
                process.kill()
            except OSError as error:
                if process.poll() is None:
                    record("process", "kill", error)

    def drain(name, stream):
        phase = "open"
        try:
            with (cwd / (prefix + "." + name)).open("xb") as target:
                while True:
                    phase = "read"
                    chunk = stream.read1(32768)
                    if not chunk:
                        break
                    remaining = limit - counts[name]
                    payload = chunk[:remaining]
                    if len(chunk) > remaining:
                        exceeded.set()
                    phase = "write"
                    written = target.write(payload)
                    if type(written) is int and 0 <= written <= len(payload):
                        counts[name] += written
                    if type(written) is not int or written != len(payload):
                        raise OSError("short capture write: expected " + str(len(payload)) +
                                      " bytes, wrote " + str(written))
                    if len(chunk) > remaining:
                        stop()
                        break
                phase = "close"
        except BaseException as error:
            record(name, phase, error)
            stop()
        finally:
            try:
                stream.close()
            except BaseException as error:
                record(name, "pipe_close", error)
                stop()

    timed_out = False
    try:
        process = subprocess.Popen([str(x) for x in command], cwd=cwd, env=env,
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except BaseException as error:
        record("process", "open", error)
    if process is not None:
        for name, stream in (("stdout", process.stdout), ("stderr", process.stderr)):
            worker = threading.Thread(target=drain, args=(name, stream), daemon=True)
            workers.append(worker)
            try:
                worker.start()
            except BaseException as error:
                workers.pop()
                record(name, "start", error)
                try:
                    stream.close()
                except BaseException as close_error:
                    record(name, "pipe_close", close_error)
                stop()
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            stop()
        except BaseException as error:
            record("process", "wait", error)
            stop()
        if code is None:
            try:
                code = process.wait(timeout=5)
            except BaseException as error:
                record("process", "reap", error)
        for worker in workers:
            worker.join(timeout=1)
        if any(worker.is_alive() for worker in workers):
            record("process", "completion", TimeoutError("capture reader did not finish"))
            stop()
            try:
                code = process.wait(timeout=5)
            except BaseException as error:
                record("process", "reap", error)
    complete = process is not None and len(workers) == 2 and all(not w.is_alive() for w in workers)
    payloads = {}
    for name in ("stdout", "stderr"):
        try:
            payloads[name] = (cwd / (prefix + "." + name)).read_bytes()
            if len(payloads[name]) != counts[name]:
                raise OSError("capture byte count mismatch")
        except BaseException as error:
            record(name, "capture_read", error)
            payloads[name] = b""
    with error_lock:
        errors = {name: list(entries) for name, entries in reader_errors.items()}
    report = {"command": [str(x) for x in command], "exit": code,
        "timed_out": timed_out, "timeout_seconds": timeout,
        "pipe_capture_complete": complete, "reader_errors": errors,
        "output_overflow": exceeded.is_set(), "stdout_bytes": counts["stdout"],
        "stderr_bytes": counts["stderr"], "limit": limit,
        "reaped": process is not None and process.poll() is not None}
    phase = "report_open"
    try:
        text = json.dumps(report, indent=2) + "\n"
        with (cwd / (prefix + ".capture.json")).open("x", encoding="utf-8", newline="\n") as target:
            phase = "report_write"
            written = target.write(text)
            if type(written) is not int or written != len(text):
                raise OSError("short capture report write")
            phase = "report_close"
    except BaseException as error:
        record("process", phase, error)
        with error_lock:
            report["reader_errors"] = {name: list(entries) for name, entries in reader_errors.items()}
    if report["reader_errors"] != {} or not complete or timed_out or exceeded.is_set() or not report["reaped"]:
        error = ValueError(prefix + " capture failed: " + json.dumps(report))
        error.capture_report = report
        raise error
    result = subprocess.CompletedProcess(command, code, payloads["stdout"], payloads["stderr"])
    result.capture_report = report
    return result


def extract_resources(jar, destination, expected):
    with zipfile.ZipFile(jar) as archive:
        for name, pin in expected.items():
            require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"),
                    "private resource allowlist")
            require(archive.getinfo(name).file_size == pin["bytes"] <= 1048576,
                    "private resource extent")
            data = archive.read(name)
            require(hashlib.sha256(data).hexdigest() == pin["sha256"], "resource digest")
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
