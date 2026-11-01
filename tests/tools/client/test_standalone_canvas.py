"""Run a real standalone Canvas with an isolated native game directory."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--profile", choices=("hidden", "visible", "visible-fp-position",
                                            "visible-fp-ungrab"), default="hidden")
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
    (output / "run.json").write_text(json.dumps({"command": command, "exit": result.returncode,
        "profile": args.profile, "status": "PASS" if result.returncode == 0 else "FAIL",
        "scope": "Authored standalone native host and checked retirement; no original comparison"},
        indent=2) + "\n", encoding="utf-8")
    print(result.stdout.decode(errors="replace"), end="")
    if result.stderr:
        print(result.stderr.decode(errors="replace"), end="")
    if result.returncode:
        raise RuntimeError(f"Canvas {args.profile} failed; artifacts: {output}")
    print(f"Canvas {args.profile} passed; artifacts: {output}")


if __name__ == "__main__":
    main()
