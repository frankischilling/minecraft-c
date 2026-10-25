"""Bounded subprocess evidence for the authored maintenance controllers."""
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


def capture(command, output, stem, timeout, environment=None):
    output = Path(output)
    exceeded = threading.Event()
    counts = {"stdout": 0, "stderr": 0}
    process = subprocess.Popen(command, env=environment, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def drain(name, stream, limit):
        with (output / (stem + "." + name)).open("xb") as target:
            while True:
                chunk = stream.read(4096)
                if not chunk:
                    break
                remaining = limit - counts[name]
                target.write(chunk[:remaining])
                counts[name] += min(len(chunk), remaining)
                if len(chunk) > remaining:
                    exceeded.set()
                    process.kill()
                    break
        stream.close()

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
        process.kill()
        code = process.wait()
    for worker in workers:
        worker.join(timeout=1)
    complete = all(not worker.is_alive() for worker in workers)
    report = {"command": [str(value) for value in command], "exit": code,
              "timeout_seconds": timeout, "timed_out": timed_out,
              "pipe_capture_complete": complete,
              "output_overflow": exceeded.is_set(),
              "stdout_file": stem + ".stdout", "stderr_file": stem + ".stderr",
              "stdout_bytes": counts["stdout"], "stderr_bytes": counts["stderr"],
              "stdout_sha256": digest(output / (stem + ".stdout")),
              "stderr_sha256": digest(output / (stem + ".stderr")),
              "stdout_limit": STDOUT_LIMIT, "stderr_limit": STDERR_LIMIT}
    (output / (stem + ".capture.json")).write_text(json.dumps(report, indent=2) + "\n")
    return report


def require_success(report, empty_stderr=False):
    ensure(report["exit"] == 0, report)
    ensure(not report["timed_out"] and not report["output_overflow"] and report["pipe_capture_complete"], report)
    if empty_stderr:
        ensure(report["stderr_bytes"] == 0, report)
