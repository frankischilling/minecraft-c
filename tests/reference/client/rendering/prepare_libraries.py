"""Provision the four pinned JVM libraries into an explicitly selected directory."""

import argparse
import json
from pathlib import Path
import sys
import urllib.request

sys.dont_write_bytecode = True
from classfile import sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    provenance = json.loads(Path(__file__).with_name("provenance.json").read_text(encoding="utf-8"))
    destination = args.output_dir.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    records = {}
    for name, expected in provenance["libraries"].items():
        path = destination / name
        url = provenance["library_urls"][name]
        if path.exists():
            content = path.read_bytes()
        else:
            with urllib.request.urlopen(url, timeout=30) as response:
                content = response.read(4 * 1024 * 1024 + 1)
            if sha256(content) != expected:
                raise ValueError(f"downloaded {name} SHA-256 differs; no file written")
            with path.open("xb") as output:
                output.write(content)
        if sha256(content) != expected:
            raise ValueError(f"existing {name} SHA-256 differs; file retained")
        records[name] = {"sha256": expected, "bytes": len(content), "source_url": url}
    with (destination / "provision.json").open("w", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(records, indent=2) + "\n")
    print(f"Verified four pinned library JARs: {destination}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
