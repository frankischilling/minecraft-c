#!/usr/bin/env python3
"""Inventory local reference JARs without extracting or publishing their contents."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import zipfile
import zlib


MAX_ARCHIVE_BYTES = 512 * 1024 * 1024
MAX_ENTRIES = 100_000
MAX_ENTRY_BYTES = 64 * 1024 * 1024
MAX_EXPANDED_BYTES = 1024 * 1024 * 1024
MAX_EXTENSION_BUCKETS = 32
MANIFEST_FIELDS = ("Manifest-Version", "Created-By", "Main-Class")


class InvalidReference(ValueError):
    """The supplied archive cannot provide a bounded, valid inventory."""


def manifest_attributes(data: bytes) -> dict[str, str]:
    """Return only selected main attributes, never per-entry digest sections."""
    text = data.decode("utf-8", errors="replace").replace("\r\n", "\n")
    logical_lines: list[str] = []
    for line in text.split("\n"):
        if not line:
            break
        if line.startswith(" ") and logical_lines:
            logical_lines[-1] += line[1:]
        else:
            logical_lines.append(line)
    attributes = {}
    for line in logical_lines:
        key, separator, value = line.partition(": ")
        if separator and key in MANIFEST_FIELDS:
            if len(value) > 1024:
                raise InvalidReference("manifest attribute exceeds 1024 characters")
            attributes[key] = value
    return attributes


def inspect_jar(path: Path) -> dict:
    size = path.stat().st_size
    if size > MAX_ARCHIVE_BYTES:
        raise InvalidReference("archive exceeds 512 MiB inspection limit")
    sha1 = hashlib.sha1()
    sha256 = hashlib.sha256()
    with path.open("rb") as archive:
        for block in iter(lambda: archive.read(1024 * 1024), b""):
            sha1.update(block)
            sha256.update(block)

    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        if len(entries) > MAX_ENTRIES:
            raise InvalidReference("archive exceeds 100000 entries")
        names = [entry.filename for entry in entries]
        if len(set(names)) != len(names):
            raise InvalidReference("duplicate archive entry names")
        files = [entry for entry in entries if not entry.is_dir()]
        if sum(entry.file_size for entry in files) > MAX_EXPANDED_BYTES:
            raise InvalidReference("archive exceeds 1 GiB expanded inspection limit")
        versions: Counter[tuple[int, int]] = Counter()
        extensions: Counter[str] = Counter()
        signature_files = 0
        signature_blocks = 0
        manifest = None
        for entry in files:
            if entry.file_size > MAX_ENTRY_BYTES:
                raise InvalidReference("entry exceeds 64 MiB inspection limit")
            if entry.flag_bits & 1:
                raise InvalidReference("encrypted archive entry")
            # Reading each entry also verifies its ZIP CRC. No files are extracted.
            data = archive.read(entry)
            if entry.filename.endswith(".class"):
                if len(data) < 10 or data[:4] != b"\xca\xfe\xba\xbe":
                    raise InvalidReference("invalid or truncated class header")
                minor, major = struct.unpack_from(">HH", data, 4)
                versions[(major, minor)] += 1
                if len(versions) > 32:
                    raise InvalidReference("archive exceeds 32 class version groups")
            else:
                suffix = Path(entry.filename).suffix.lower()
                bucket = suffix if suffix and len(suffix) <= 16 else "[no short suffix]"
                extensions[bucket] += 1
            upper_name = entry.filename.upper()
            if upper_name == "META-INF/MANIFEST.MF":
                manifest = manifest_attributes(data)
            if upper_name.startswith("META-INF/"):
                suffix = Path(upper_name).suffix
                signature_files += suffix == ".SF"
                signature_blocks += suffix in (".RSA", ".DSA", ".EC")
        if not versions:
            raise InvalidReference("archive contains no class files")
        if manifest is None:
            raise InvalidReference("archive has no JAR manifest")

    common_extensions = sorted(extensions.items(), key=lambda item: (-item[1], item[0]))
    bounded_extensions = dict(common_extensions[:MAX_EXTENSION_BUCKETS])
    if len(common_extensions) > MAX_EXTENSION_BUCKETS:
        bounded_extensions["[other suffixes]"] = sum(
            count for _, count in common_extensions[MAX_EXTENSION_BUCKETS:]
        )
    class_count = sum(versions.values())
    return {
        "filename": path.name,
        "size_bytes": size,
        "sha1": sha1.hexdigest(),
        "sha256": sha256.hexdigest(),
        "entries": len(entries),
        "files": len(files),
        "directories": len(entries) - len(files),
        "expanded_bytes": sum(entry.file_size for entry in files),
        "classes": class_count,
        "resources_including_manifest_and_signatures": len(files) - class_count,
        "class_file_versions": [
            {"major": major, "minor": minor, "count": count}
            for (major, minor), count in sorted(versions.items())
        ],
        "resource_suffix_counts": bounded_extensions,
        "manifest_main_attributes": manifest,
        "signature_metadata": {
            "signature_file_count": signature_files,
            "signature_block_count": signature_blocks,
            "cryptographic_verification": "not performed by this inspector",
        },
        "zip_crc": "passed for every file",
        "class_validation": "magic and version headers only; not JVM verification",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    references = {"client": args.client, "server": args.server}
    output = args.output.resolve()
    if any(output == path.resolve() for path in references.values()):
        print("INVALID: output must not overwrite a reference JAR", file=sys.stderr)
        return 2
    missing = [side for side, path in references.items() if not path.is_file()]
    if missing:
        print("UNAVAILABLE: missing local reference input(s): " + ", ".join(missing), file=sys.stderr)
        return 77
    inventories = {}
    for side, path in references.items():
        try:
            inventories[side] = inspect_jar(path)
        except (OSError, ValueError, RuntimeError, zipfile.BadZipFile,
                NotImplementedError, EOFError, zlib.error) as error:
            print(f"INVALID: {side} reference: {error}", file=sys.stderr)
            return 2
    result = {
        "schema_version": 1,
        "scope": "local binary metadata; no runtime or interoperability verification",
        "inputs": inventories,
    }
    temporary = None
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", dir=output.parent,
            prefix=".jar-inventory-", suffix=".tmp", delete=False,
        ) as stream:
            temporary = Path(stream.name)
            json.dump(result, stream, indent=2, sort_keys=True)
            stream.write("\n")
        os.replace(temporary, output)
    except OSError as error:
        print(f"INVALID: cannot write inventory: {error}", file=sys.stderr)
        return 2
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print("AVAILABLE: inspected client and server; wrote bounded metadata inventory")
    return 0


if __name__ == "__main__":
    sys.exit(main())
