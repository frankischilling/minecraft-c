"""Print bounded terrain-test scopes and captured failures in CI."""

import argparse
import json
from pathlib import Path


GROUPS = (
    "render-batch-cpu", "render-batch-device", "render-global-recenter",
    "render-global-recenter-device", "render-global-occlusion",
    "render-global-submit", "render-global-submit-cpu",
    "lighting-enable",
)


def captures(value):
    if isinstance(value, dict):
        if "exit" in value and "stderr_file" in value:
            yield value
        for child in value.values():
            yield from captures(child)
    elif isinstance(value, list):
        for child in value:
            yield from captures(child)


def excerpt(directory, name, limit, tail=False):
    path = (directory / name).resolve()
    if not path.is_relative_to(directory.resolve()) or not path.is_file():
        print(f"  unavailable capture file: {name}")
        return
    with path.open("rb") as source:
        if tail and path.stat().st_size > limit:
            source.seek(-limit, 2)
        data = source.read(limit + 1)
    if data:
        print(f"  {path.name} (at most {limit} bytes):")
        print(data[:limit].decode("utf-8", errors="replace"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    found = 0
    for group in GROUPS:
        reports = sorted((build / group).glob("*/run.json"),
                         key=lambda path: path.stat().st_mtime, reverse=True)
        if not reports:
            continue
        found += 1
        path = reports[0]
        try:
            report = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            print(f"{group}: unreadable receipt: {error}")
            continue
        keys = ("status", "rows", "cpu_rows", "device_rows", "original_rows", "compared_rows", "profiles_completed",
                "unsupported_profiles", "uncredited_rows")
        print(f"{group}: " + json.dumps({key: report[key] for key in keys if key in report}))
        for profile in report.get("profiles", []):
            fields = ("name", "status", "capability", "device", "unsupported", "uncredited_rows")
            print("  profile: " + json.dumps({key: profile[key] for key in fields if key in profile}))
            if profile.get("status") == "UNSUPPORTED" and "context" in profile:
                print("  unsupported context: " + json.dumps(profile["context"]))
            if profile.get("mismatches"):
                print("  profile mismatches: " + json.dumps(profile["mismatches"][:4])[:4096])
        if report.get("mismatches"):
            print("  mismatches: " + json.dumps(report["mismatches"][:4])[:4096])
        if report.get("status") not in ("PASS", "PASS_WITH_UNSUPPORTED"):
            for index, capture in enumerate(captures(report)):
                if index >= 16:
                    print("  further capture excerpts omitted")
                    break
                excerpt(path.parent, capture["stderr_file"], 8192)
                excerpt(path.parent, capture["stdout_file"], 4096, tail=True)
    log = build / "Testing/Temporary/LastTest.log"
    if log.is_file():
        prefixes = ("Source N submit:", "Source N submit unsupported:", "Source submission CPU:",
                    "Source recenter/JO:", "Source batches:", "Source ARB queries:",
                    "Source U.b lighting:", "Source lighting controls:")
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith(prefixes):
                print("  completion: " + line)
    if not found:
        print("No terrain receipts were generated; no device execution is credited.")


if __name__ == "__main__":
    main()
