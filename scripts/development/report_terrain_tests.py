"""Print bounded terrain and lighting scopes and captured failures in CI."""

import argparse
import json
from pathlib import Path
import re


GROUPS = (
    "render-batch-cpu", "render-batch-device", "render-global-recenter",
    "render-global-recenter-device", "render-global-occlusion",
    "render-global-submit", "render-global-submit-cpu",
    "lighting-enable",
    "lighting-material", "lighting-list-state", "lighting-array-normals",
    "lighting-material-sequences", "lighting-color-inputs",
    "lighting-raster", "lighting-raster-extended", "lighting-raster-converted",
)


def bounded_text(value, limit=1024):
    # A stdout tail may begin in the middle of a complete pixel row. Suppress
    # long encoded tokens even when the RASTER prefix is outside the excerpt.
    value = re.sub(r"[A-Za-z0-9+/_=-]{256,}",
                   lambda match: f"<encoded token omitted: {len(match[0])} characters>", str(value))
    return value if len(value) <= limit else value[:limit] + " <truncated>"


def bounded(value):
    if isinstance(value, str):
        return bounded_text(value)
    if isinstance(value, dict):
        return {key: bounded(child) for key, child in value.items()}
    if isinstance(value, list):
        return [bounded(child) for child in value[:64]] + (
            [f"<{len(value) - 64} further values omitted>"] if len(value) > 64 else [])
    return value


def print_json(label, value):
    print(label + json.dumps(bounded(value)))


def report_mismatches(label, value):
    if not value:
        return
    if not isinstance(value, list):
        print_json(label, value)
        return
    print_json(label + "count: ", len(value))
    for difference in value[:4]:
        if isinstance(difference, dict) and "different_pixels" in difference:
            keys = ("index", "kind", "frame", "different_pixels", "different_channels",
                    "maximum_channel_difference", "native_sha256", "original_sha256")
            detail = {key: difference[key] for key in keys if key in difference}
            detail["examples"] = difference.get("examples", [])[:2]
        else:
            detail = difference
        print_json(label, detail)
    if len(value) > 4:
        print(f"  {len(value) - 4} further mismatch rows omitted")


def failed_capture(capture):
    return (capture.get("exit") != 0 or capture.get("timed_out") or
            capture.get("output_overflow") or not capture.get("pipe_capture_complete", True) or
            capture.get("reader_errors") or capture.get("stderr_bytes", 0) != 0)


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
        print(bounded_text(data[:limit].decode("utf-8", errors="replace"), limit))


def native_headers(directory, report):
    seen = set()
    for capture in captures(report):
        name = capture.get("stdout_file")
        if not isinstance(name, str):
            continue
        path = (directory / name).resolve()
        if not path.is_relative_to(directory.resolve()) or not path.is_file():
            continue
        try:
            with path.open("rb") as source:
                text = source.read(4096).decode("utf-8", errors="replace")
        except OSError as error:
            print("  unavailable native context: " + bounded_text(error))
            continue
        for line in text.splitlines():
            if line.startswith(("RASTER_NATIVE_DRIVER ", "RASTER_NATIVE_FORMAT ")) and line not in seen:
                seen.add(line)
                print(f"  native context ({path.name}): " + bounded_text(line))
        if len(seen) >= 8:
            print("  further distinct native context headers omitted")
            break


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
        keys = ("status", "rows", "frames", "normal_rows", "state_rows", "vertices", "components",
                "cpu_rows", "device_rows", "original_rows", "compared_rows", "profiles_completed",
                "unsupported_profiles", "uncredited_rows", "complete_inventory", "selected_indices",
                "reference_profile", "direct_rows", "list_rows", "contexts")
        summary = {key: report[key] for key in keys if key in report}
        if "profiles" in report:
            summary["profiles_recorded"] = len(report["profiles"])
            summary["profiles_compared"] = sum(profile.get("status") in ("PASS", "FAIL")
                                               for profile in report["profiles"])
        print_json(f"{group}: ", summary)
        if group.startswith("lighting-"):
            native_headers(path.parent, report)
        for key in ("scope", "failure", "error", "gpu_layout"):
            if key in report:
                print_json(f"  {key}: ", report[key])
        for profile in report.get("profiles", []):
            fields = ("name", "index", "status", "rows", "frames", "capability", "device",
                      "unsupported", "uncredited_rows", "failure", "error")
            detail = {key: profile[key] for key in fields if key in profile}
            detail.setdefault("status", "INCOMPLETE")
            print_json("  profile: ", detail)
            if profile.get("status") == "UNSUPPORTED" and "context" in profile:
                print_json("  unsupported context: ", profile["context"])
            report_mismatches("  profile mismatches: ", profile.get("mismatches"))
        report_mismatches("  mismatches: ", report.get("mismatches"))
        if report.get("status") not in ("PASS", "PASS_WITH_UNSUPPORTED"):
            lighting = group.startswith("lighting-")
            diagnosed = bool(report.get("mismatches")) or any(
                profile.get("mismatches") for profile in report.get("profiles", []))
            incomplete = {id(capture) for profile in report.get("profiles", [])
                          if profile.get("status") not in ("PASS", "FAIL", "UNSUPPORTED")
                          for capture in captures(profile)}
            available = list(captures(report))
            if lighting:
                important = [capture for capture in available
                             if failed_capture(capture) or id(capture) in incomplete]
                if important:
                    available = important
                elif diagnosed:
                    available = []
            shown = 0
            for capture in available:
                if shown >= 16:
                    print("  further capture excerpts omitted")
                    break
                shown += 1
                fields = ("stdout_file", "stderr_file", "exit", "timed_out", "timeout_seconds",
                          "output_overflow", "pipe_capture_complete", "reader_errors",
                          "stdout_bytes", "stderr_bytes")
                print_json("  process: ", {key: capture[key] for key in fields if key in capture})
                excerpt(path.parent, capture["stderr_file"], 8192)
                excerpt(path.parent, capture["stdout_file"], 4096, tail=True)
    log = build / "Testing/Temporary/LastTest.log"
    if log.is_file():
        prefixes = ("Source N submit:", "Source N submit unsupported:", "Source submission CPU:",
                    "Source recenter/JO:", "Source batches:", "Source ARB queries:",
                    "Source U.b lighting:", "Source lighting controls:",
                    "Source material tracking:", "Source lighting material:",
                    "Source lighting list attributes:", "Source lighting list state:",
                    "Source lighting array normals:", "Source lighting raster:",
                    "Source extended lighting raster:", "Source actual converted lighting raster:")
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith(prefixes):
                print("  completion: " + bounded_text(line))
    if not found:
        print("No terrain receipts were generated; no device execution is credited.")


if __name__ == "__main__":
    main()
