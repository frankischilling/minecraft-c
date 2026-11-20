"""Run a real standalone Canvas with an isolated native game directory."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--profile", choices=("hidden", "visible", "visible-fp-position",
                                            "visible-fp-ungrab", "visible-reactivation"), default="hidden")
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.profile + "-", dir=args.work_dir.resolve()))
    environment = os.environ.copy()
    environment["ALSOFT_DRIVERS"] = "null"
    for variable in ("APPDATA", "LOCALAPPDATA", "USERPROFILE", "HOME", "XDG_DATA_HOME",
                     "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
        directory = output / variable.lower()
        directory.mkdir()
        environment[variable] = str(directory)
    command = [str(args.executable.resolve(strict=True)), str(args.assets.resolve(strict=True)),
               args.profile]
    result = subprocess.run(command, cwd=output, env=environment, capture_output=True, timeout=150)
    (output / "native.stdout").write_bytes(result.stdout)
    (output / "native.stderr").write_bytes(result.stderr)
    report = {"command": command, "exit": result.returncode,
              "profile": args.profile, "status": "FAIL",
              "scope": "Authored standalone native host and checked retirement; no original comparison"}
    print(result.stdout.decode(errors="replace"), end="")
    if result.stderr:
        print(result.stderr.decode(errors="replace"), end="")
    try:
        if result.returncode:
            raise RuntimeError(f"Canvas {args.profile} failed; artifacts: {output}")
        if args.profile == "visible-reactivation":
            rows = [line for line in result.stdout.decode("utf-8", "replace").splitlines()
                    if line.startswith("CANVAS reactivation cycles=")]
            expression = (r"CANVAS reactivation cycles=([0-9]+) sibling-visible=1 native-canvas-focus=1 "
                          r"SDK-canvas-focus=1 frame-only-reactivation=1 passive-pumps=8 relative-grab=1 "
                          r"keyboard=2 warp-capability=([01]) native-clip=(win32|not-observed) "
                          r"checked-owner-retirement=1")
            match = re.fullmatch(expression, rows[0]) if len(rows) == 1 else None
            if match is None or not 2 <= int(match[1]) <= 64:
                raise RuntimeError(f"Canvas real reactivation receipt missing; artifacts: {output}")
            if match[3] != ("win32" if os.name == "nt" else "not-observed"):
                raise RuntimeError(f"Canvas native clip observation scope; artifacts: {output}")
    except Exception as error:
        report["error"] = str(error)
        raise
    else:
        report["status"] = "PASS"
    finally:
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Canvas {args.profile} passed; artifacts: {output}")


if __name__ == "__main__":
    main()
