"""Run concrete World Player publication on authored resources and fresh Worlds."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import threading


def run(command, root):
    chunks = [[], []]
    overflow = threading.Event()
    with subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE) as process:
        def collect(pipe, index):
            count = 0
            while data := pipe.read(8192):
                count += len(data)
                if count > 65536:
                    overflow.set()
                    process.kill()
                    return
                chunks[index].append(data)
        threads = [threading.Thread(target=collect, args=(pipe, index), daemon=True)
                   for index, pipe in enumerate((process.stdout, process.stderr))]
        for thread in threads:
            thread.start()
        try:
            code = process.wait(timeout=90)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise RuntimeError("World Player publication timeout") from None
        finally:
            for thread in threads:
                thread.join(timeout=5)
    output, error = (b"".join(parts) for parts in chunks)
    (root / "native.stdout").write_bytes(output)
    (root / "native.stderr").write_bytes(error)
    if overflow.is_set() or any(thread.is_alive() for thread in threads):
        raise RuntimeError("World Player publication capture bound/shutdown")
    return code, output, error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="publication-", dir=args.work_dir.resolve()))
    resources = root / "resources"
    for name, value in {
            "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                               b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
            "lang/stats_US.lang": b"# Authored statistics resource\n",
            "achievement/map.txt": b""}.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(value)
    command = [str(args.executable.resolve()), str(resources), str(root / "normal-world"),
               str(root / "zero-capacity-world")]
    code, output, error = run(command, root)
    lines = output.splitlines()
    count = re.fullmatch(rb"World Player publication: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    expected = [b"151 recipes", b"16 achievements"]
    for index, phase in enumerate(("attached", "duplicate", "null-access"), 1):
        expected.append((f"PUB\t{phase}\td={index}\tb={index}\tJ=false\tslice={index}"
                         "\tmember=true\tx=0\ty=4\tz=0").encode("ascii"))
    expected += [b"151 recipes", b"16 achievements"]
    if code != 0 or error or count is None or lines[:-1] != expected:
        raise RuntimeError("World Player publication failed; captures: " + str(root))
    report = {"status": "passed", "checks": int(count.group(1)), "exit": code,
              "stderr_bytes": len(error), "run_directory": str(root)}
    (root / "report.json").write_bytes((json.dumps(report, indent=2) + "\n").encode("ascii"))
    print(json.dumps(report))


if __name__ == "__main__":
    main()
