"""Run the genuine native controller contract and retain process completion."""
import argparse
import json
from pathlib import Path
import re
import tempfile

from capture import capture, digest, ensure, require_success
from capture_reference import rows

RESOURCES = {
    "lang/en_US.lang": (b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                        b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n"),
    "lang/stats_US.lang": b"# Authored statistics resource\n",
    "achievement/map.txt": b"",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--work-dir", type=Path)
    options = parser.parse_args()
    if options.work_dir is not None:
        options.work_dir.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="singleplayer-controller-", dir=options.work_dir))
    else:
        output = options.output
        output.mkdir(parents=True, exist_ok=False)
    resources = output / "resources"
    for name, raw in RESOURCES.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    worlds = output / "worlds"
    worlds.mkdir()
    executable = options.executable.resolve(strict=True)
    before = digest(executable)
    report = capture([str(executable), str(resources.resolve()), str(worlds.resolve())],
                     output, "native", 180)
    require_success(report, empty_stderr=True)
    after = digest(executable)
    ensure(before == after, "native executable changed during execution")
    text = (output / "native.stdout").read_text()
    ensure(len(rows(text)) == 20, "native controller records differ")
    lines = text.splitlines()
    match = re.fullmatch(r"PASS: ([1-9][0-9]*) single-player controller checks", lines[-1])
    ensure(match is not None, "native controller terminal marker missing")
    report.update(executable_sha256=before, executable_before_sha256=before,
                  executable_after_sha256=after, executable_unchanged=True,
                  mode="controller", checks=int(match[1]),
                  authored_resources={name: digest(resources / name) for name in RESOURCES})
    (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("Native controller: " + str(report["checks"]) + " checks; report=" + str(output / "run.json"))


if __name__ == "__main__":
    main()
