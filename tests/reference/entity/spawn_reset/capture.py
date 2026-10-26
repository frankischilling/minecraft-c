"""Bounded subprocess evidence for the authored action controllers."""
import hashlib
import json
from pathlib import Path
import subprocess
import threading

STDOUT_LIMIT = 4 * 1024 * 1024
STDERR_LIMIT = 1024 * 1024


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ensure(condition, message):
    if not condition:
        raise AssertionError(message)


def capture(command, output, stem, timeout, environment=None, working_directory=None):
    output = Path(output)
    exceeded = threading.Event()
    counts = {"stdout": 0, "stderr": 0}
    reader_errors = {}
    error_lock = threading.Lock()
    process = subprocess.Popen(command, env=environment, cwd=working_directory, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def record_error(name, phase, error):
        with error_lock:
            reader_errors.setdefault(name, []).append({"phase": phase,
                "type": type(error).__name__, "message": str(error)})

    def stop_process():
        try:
            process.kill()
        except OSError as error:
            # Another reader, wait() or the child may already have won the
            # exit race. A still-live child whose kill failed is evidence.
            if process.poll() is None:
                record_error("process", "kill", error)

    def drain(name, stream, limit):
        phase = "open"
        try:
            with (output / (stem + "." + name)).open("xb") as target:
                while True:
                    phase = "read"
                    chunk = stream.read(4096)
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
                        stop_process()
                        break
                phase = "close"
        except BaseException as error:
            record_error(name, phase, error)
            stop_process()
        finally:
            try:
                stream.close()
            except BaseException as error:
                record_error(name, "pipe_close", error)
                stop_process()

    workers = [threading.Thread(target=drain, args=(name, stream, limit), daemon=True)
               for name, stream, limit in [("stdout", process.stdout, STDOUT_LIMIT),
                                            ("stderr", process.stderr, STDERR_LIMIT)]]
    for worker in workers:
        worker.start()
    timed_out = False
    try:
        code = process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        stop_process()
        try:
            code = process.wait(timeout=5)
        except subprocess.TimeoutExpired as error:
            record_error("process", "reap", error)
            code = process.poll()
    for worker in workers:
        worker.join(timeout=1)
    complete = all(not worker.is_alive() for worker in workers)
    if not complete:
        for name, worker in zip(("stdout", "stderr"), workers):
            if worker.is_alive():
                record_error(name, "completion", TimeoutError("capture reader did not finish"))
        stop_process()

    def captured_digest(name):
        try:
            return digest(output / (stem + "." + name))
        except BaseException as error:
            record_error(name, "digest", error)
            stop_process()
            return None

    hashes = {name: captured_digest(name) for name in ("stdout", "stderr")}
    with error_lock:
        errors = {name: list(entries) for name, entries in reader_errors.items()}
    report = {"command": [str(value) for value in command], "exit": code,
              "timeout_seconds": timeout, "timed_out": timed_out,
              "pipe_capture_complete": complete,
              "reader_errors": errors,
              "output_overflow": exceeded.is_set(),
              "stdout_file": stem + ".stdout", "stderr_file": stem + ".stderr",
              "stdout_bytes": counts["stdout"], "stderr_bytes": counts["stderr"],
              "stdout_sha256": hashes["stdout"],
              "stderr_sha256": hashes["stderr"],
              "stdout_limit": STDOUT_LIMIT, "stderr_limit": STDERR_LIMIT}
    (output / (stem + ".capture.json")).write_text(json.dumps(report, indent=2) + "\n")
    return report


def require_success(report, empty_stderr=False):
    ensure(report.get("reader_errors") == {}, report)
    ensure(report["exit"] == 0, report)
    ensure(not report["timed_out"] and not report["output_overflow"] and report["pipe_capture_complete"], report)
    if empty_stderr:
        ensure(report["stderr_bytes"] == 0, report)


def validate_native_result(stdout_path, receipt_path, mode):
    """Require completed native execution separately from its numeric rows."""
    import re
    ensure(receipt_path is not None, "native stdout requires a native process receipt")
    receipt_path = Path(receipt_path).resolve(strict=True)
    stdout_path = Path(stdout_path).resolve(strict=True)
    report = json.loads(receipt_path.read_text())
    require_success(report, empty_stderr=True)
    ensure(type(report["exit"]) is int and report["exit"] == 0, "native process did not exit successfully")
    ensure(report["timed_out"] is False and report["output_overflow"] is False and
           report["pipe_capture_complete"] is True, "native process capture is incomplete")
    streams = {}
    for name in ["stdout", "stderr"]:
        filename = report[name + "_file"]
        ensure(isinstance(filename, str) and Path(filename).name == filename,
               "native stream receipt must name a local capture file")
        path = receipt_path.parent / filename
        ensure(path.is_file() and path.stat().st_size == report[name + "_bytes"] and
               digest(path) == report[name + "_sha256"], "native stream changed after capture")
        streams[name] = path.resolve()
    ensure(streams["stdout"] == stdout_path, "native receipt describes another stdout file")
    ensure(report["mode"] == mode, "native mode differs")
    ensure(report["executable_unchanged"] is True, "native executable was not stable")
    command = report["command"]
    ensure(isinstance(command, list) and command and all(isinstance(v, str) for v in command),
           "native process command is missing")
    executable = Path(command[0]).resolve(strict=True)
    executable_digest = digest(executable)
    ensure(report["executable_before_sha256"] == report["executable_after_sha256"] ==
           report["executable_sha256"] == executable_digest, "native executable fingerprint differs")
    suffix = "position vector checks" if mode == "position-probe" else "Player spawn reset checks"
    lines = stdout_path.read_text().splitlines()
    marker = re.fullmatch(r"PASS: ([1-9][0-9]*) " + suffix, lines[-1]) if lines else None
    ensure(marker is not None and type(report["checks"]) is int and
           int(marker[1]) == report["checks"], "native terminal marker or check count differs")
    return {"receipt_sha256": digest(receipt_path), "exit": report["exit"], "mode": mode,
            "checks": report["checks"], "executable_sha256": executable_digest,
            "stdout_sha256": report["stdout_sha256"], "stderr_sha256": report["stderr_sha256"],
            "inputs": [str(receipt_path), str(stdout_path), str(streams["stderr"]), str(executable)]}
