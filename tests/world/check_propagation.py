"""Run real native World lighting, retain bounded output and require its proof."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import threading

STDOUT_LIMIT = 4 * 1024 * 1024
STDERR_LIMIT = 256 * 1024
SUCCESS = re.compile(rb"Lighting propagation: ([1-9][0-9]*) checks")


def run_checked(command, directory, timeout=120):
    """Capture bounded byte streams; a child exceeding either bound is killed."""
    overflow = threading.Event()
    sizes = {"stdout": 0, "stderr": 0}
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def capture(name, stream, maximum):
        with (directory / (name + ".log")).open("wb") as target:
            while True:
                data = stream.read(4096)
                if not data:
                    break
                available = maximum - sizes[name]
                target.write(data[:available])
                sizes[name] += min(len(data), available)
                if len(data) > available:
                    overflow.set()
                    process.kill()
                    break
        stream.close()

    readers = [threading.Thread(target=capture, args=(name, stream, maximum))
               for name, stream, maximum in [("stdout", process.stdout, STDOUT_LIMIT),
                                              ("stderr", process.stderr, STDERR_LIMIT)]]
    for thread in readers:
        thread.start()
    timed_out = False
    try:
        code = process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        process.kill()
        code = process.wait()
    for thread in readers:
        thread.join()
    stdout = (directory / "stdout.log").read_bytes()
    stderr = (directory / "stderr.log").read_bytes()
    lines = stdout.splitlines()
    final = SUCCESS.fullmatch(lines[-1]) if lines else None
    checks = int(final.group(1)) if final is not None else 0
    failures = []
    if code != 0:
        failures.append("native process did not exit successfully")
    if timed_out:
        failures.append("native process timed out")
    if overflow.is_set():
        failures.append("native output exceeded the capture limit")
    if stderr:
        failures.append("native stderr is not empty")
    if checks == 0:
        failures.append("missing positive final Lighting propagation check count")
    report = {"command": command, "exit": code, "checks": checks,
              "stdout_bytes": len(stdout), "stderr_bytes": len(stderr),
              "stdout_limit": STDOUT_LIMIT, "stderr_limit": STDERR_LIMIT,
              "timeout_seconds": timeout, "timed_out": timed_out,
              "output_overflow": overflow.is_set(), "failures": failures}
    (directory / "run.json").write_text(json.dumps(report, indent=2) + "\n")
    # Preserve source diagnostics, including intentional oversized light-region
    # messages, without treating ordinary stdout as an error stream.
    if stdout:
        print(stdout.decode("utf-8", errors="replace"), end="")
    if stderr:
        print(stderr.decode("utf-8", errors="replace"), end="")
    if failures:
        print("Lighting propagation driver failed: " + "; ".join(failures))
        print("Captured output: " + str(directory))
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--startup-grid", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="propagation-", dir=args.output))
    resources = root / "resources"
    language = resources / "lang"
    language.mkdir(parents=True)
    (resources / "achievement").mkdir()
    (resources / "achievement" / "map.txt").write_bytes(b"")
    (language / "en_US.lang").write_bytes(b"tile.stone.name=Authored stone\n")
    (language / "stats_US.lang").write_bytes(
        b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
        b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")
    command = [args.executable, str(resources), str(root / "world"), str(root)]
    if args.startup_grid:
        command.append("--startup-grid")
    return run_checked(command, root)


if __name__ == "__main__":
    raise SystemExit(main())
