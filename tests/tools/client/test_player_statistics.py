"""Run real Player/App/Session/xi/vu dispatch owners using authored resources."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--fault-executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / "reference/client/player_statistics/support.py"
    spec = importlib.util.spec_from_file_location("mc_player_statistics_support", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Player statistics support unavailable")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="player-statistics-", dir=args.work_dir.resolve()))
    resources = root / "resources"
    support.resources_at(resources)
    report = {"status": "PASS", "profiles": {}, "run_directory": str(root)}
    for label, executable, fault in (("native", args.executable, False),
                                     ("clock-leaf-fault", args.fault_executable, True)):
        executable = executable.resolve(strict=True)
        game = root / label
        game.mkdir()
        result = support.run([executable, resources, game], root, label)
        support.require(result.returncode == 0 and result.stderr == b"", label + " native failure")
        rows, checks = support.rows(result.stdout, native=True, fault=fault)
        report["profiles"][label] = {"checks": checks, "rows": len(rows), "exit": 0,
            "stderr_bytes": 0, "capture": result.capture_report}
        for bad, command in (("argc", [executable]),
            ("directory", [executable, resources, root / "absent"])):
            rejected = support.run(command, root, label + "-" + bad)
            support.require(rejected.returncode == 2 and rejected.stdout == b"" and
                rejected.stderr.replace(b"\r\n", b"\n") ==
                b"Player statistics input: resources work-directory\n", "exact input rejection " + label + bad)
    (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
