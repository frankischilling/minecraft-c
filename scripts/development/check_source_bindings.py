"""Check asset-independent current test inputs before configuring a native build."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

PROFILES = (
    ("application_owner", ("native_sources", "reference_files", "controller_files", "shared_helpers")),
    ("biped", ("native_sources", "reference_files", "controller_files", "shared_helpers")),
    ("render_resources", ("source_files",)),
    ("display_sdk", ("source_files",)),
    ("loading_draw", ("native_sources",)),
    ("resource_lookup_common", ("source_files",)),
)


def check(root):
    root = root.resolve(strict=True)
    observed = {}
    count = 0
    for profile, groups in PROFILES:
        metadata = root / "tests/reference/client" / profile / "provenance.json"
        proof = json.loads(metadata.read_text(encoding="utf-8"))
        for group in groups:
            files = proof.get(group)
            if type(files) is not dict or not files:
                raise ValueError(f"{profile}.{group}: expected declared input table")
            for relative, expected in files.items():
                if (type(expected) is not dict or set(expected) != {"bytes", "sha256"} or
                        type(expected["bytes"]) is not int or expected["bytes"] < 0 or
                        type(expected["sha256"]) is not str or
                        re.fullmatch(r"[0-9a-f]{64}", expected["sha256"]) is None):
                    raise ValueError(f"{profile}.{group}: invalid binding for {relative}")
                path = (root / relative).resolve(strict=True)
                if not path.is_relative_to(root):
                    raise ValueError(f"{profile}.{group}: path outside source root: {relative}")
                if path not in observed:
                    data = path.read_bytes()
                    observed[path] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                if observed[path] != expected:
                    raise ValueError(f"{profile}.{group}: stale source binding: {relative}")
                count += 1
    print(f"Source bindings: {count} entries, {len(observed)} distinct current files")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    try:
        check(args.root)
    except (OSError, ValueError) as error:
        print(f"Source bindings failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
