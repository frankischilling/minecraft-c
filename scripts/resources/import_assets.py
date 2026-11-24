#!/usr/bin/env python3
"""Import user-owned Beta 1.7.3 resources into a fresh local directory.

This development tool has no network access and never extracts classes or launchers.
The native client reads the imported files; this tool only validates and copies them.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import stat
import sys
import tempfile
from typing import BinaryIO
import zipfile


CLIENT_SHA256 = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
MAX_JAR_BYTES = 16 * 1024 * 1024
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_TOTAL_BYTES = 256 * 1024 * 1024
MAX_ENTRIES = 8192
CHUNK_BYTES = 64 * 1024

# All 83 non-class resources in the verified client, except its empty "null" entry.
JAR_RESOURCES = tuple("""
font.txt pack.png pack.txt particles.png terrain.png
achievement/bg.png achievement/icons.png achievement/map.txt
armor/chain_1.png armor/chain_2.png armor/cloth_1.png armor/cloth_2.png
armor/diamond_1.png armor/diamond_2.png armor/gold_1.png armor/gold_2.png
armor/iron_1.png armor/iron_2.png armor/power.png art/kz.png
environment/clouds.png environment/rain.png environment/snow.png font/default.png
gui/background.png gui/container.png gui/crafting.png gui/furnace.png gui/gui.png
gui/icons.png gui/inventory.png gui/items.png gui/logo.png gui/particles.png
gui/slot.png gui/trap.png gui/unknown_pack.png
item/arrows.png item/boat.png item/cart.png item/door.png item/sign.png
lang/en_US.lang lang/stats_US.lang
misc/dial.png misc/foliagecolor.png misc/footprint.png misc/grasscolor.png
misc/mapbg.png misc/mapicons.png misc/pumpkinblur.png misc/shadow.png
misc/vignette.png misc/water.png misc/watercolor.png
mob/char.png mob/chicken.png mob/cow.png mob/creeper.png mob/ghast.png
mob/ghast_fire.png mob/pig.png mob/pigman.png mob/pigzombie.png mob/saddle.png
mob/sheep.png mob/sheep_fur.png mob/silverfish.png mob/skeleton.png mob/slime.png
mob/spider.png mob/spider_eyes.png mob/squid.png mob/wolf.png mob/wolf_angry.png
mob/wolf_tame.png mob/zombie.png terrain/moon.png terrain/sun.png
title/black.png title/mclogo.png title/mojang.png title/splashes.txt
""".split())

RESOURCE_GROUPS = ("sound", "newsound", "streaming", "music", "newmusic")
# An availability floor observed in the supplied local resources, not a canonical
# manifest of every Beta sound or proof that externally supplied audio is original.
REQUIRED_EXTERNAL = (
    "sound/step/stone1.ogg", "newsound/random/click.ogg", "music/calm1.ogg",
    "newmusic/hal1.ogg", "streaming/13.ogg", "streaming/cat.ogg",
)


class ImportFailure(Exception):
    """An input or destination failed a local import precondition."""


def safe_relative(name: str) -> Path:
    """Validate portable resource paths before joining them to a filesystem root."""
    parts = name.split("/")
    devices = {"con", "prn", "aux", "nul"} | {
        f"{prefix}{number}" for prefix in ("com", "lpt") for number in range(1, 10)
    }
    if not name or "\\" in name or PurePosixPath(name).is_absolute():
        raise ImportFailure(f"Unsafe resource path: {name!r}")
    for part in parts:
        if (part in ("", ".", "..") or part.endswith((".", " "))
                or any(ord(char) < 32 or char in '<>:"|?*' for char in part)
                or part.split(".")[0].lower() in devices):
            raise ImportFailure(f"Unsafe resource path: {name!r}")
    return Path(*parts)


def plain_path(path: Path, *, directory: bool) -> os.stat_result:
    """Reject symlinks, junctions, reparse points, and special files."""
    try:
        result = path.lstat()
    except FileNotFoundError as error:
        raise ImportFailure(f"Missing {'directory' if directory else 'file'}: {path}") from error
    if (stat.S_ISLNK(result.st_mode)
            or getattr(result, "st_file_attributes", 0) & 0x400):
        raise ImportFailure(f"Links and reparse points are not accepted: {path}")
    expected = stat.S_ISDIR if directory else stat.S_ISREG
    if not expected(result.st_mode):
        raise ImportFailure(f"Expected {'directory' if directory else 'regular file'}: {path}")
    return result


def check_ancestors(path: Path) -> None:
    """Check existing components without resolving away links before inspecting."""
    for ancestor in reversed((path, *path.parents)):
        if ancestor.exists() or ancestor.is_symlink():
            plain_path(ancestor, directory=True)


def copy_bounded(source: BinaryIO, destination: BinaryIO, expected: int) -> str:
    if not 0 < expected <= MAX_FILE_BYTES:
        raise ImportFailure(f"Resource size outside 1..{MAX_FILE_BYTES}: {expected}")
    digest = hashlib.sha256()
    remaining = expected
    while remaining:
        data = source.read(min(CHUNK_BYTES, remaining))
        if not data:
            raise ImportFailure("Resource ended before its declared size")
        destination.write(data)
        digest.update(data)
        remaining -= len(data)
    if source.read(1):
        raise ImportFailure("Resource exceeds its declared size")
    return digest.hexdigest()


def verify_client_jar(path: Path) -> str:
    check_ancestors(path.parent)
    size = plain_path(path, directory=False).st_size
    if not 0 < size <= MAX_JAR_BYTES:
        raise ImportFailure(f"Client JAR exceeds the {MAX_JAR_BYTES}-byte limit or is empty")
    digest = hashlib.sha256()
    with path.open("rb") as source:
        remaining = size
        while remaining:
            data = source.read(min(CHUNK_BYTES, remaining))
            if not data:
                raise ImportFailure("Client JAR changed while hashing")
            digest.update(data)
            remaining -= len(data)
        if source.read(1):
            raise ImportFailure("Client JAR changed while hashing")
    actual = digest.hexdigest()
    if actual != CLIENT_SHA256:
        raise ImportFailure(
            f"Unsupported client JAR SHA-256 {actual}; expected Beta 1.7.3 {CLIENT_SHA256}"
        )
    return actual


def inspect_jar(archive: zipfile.ZipFile) -> list[zipfile.ZipInfo]:
    """Inspect metadata only; tests can exercise this with synthetic archives."""
    entries = archive.infolist()
    if len(entries) > MAX_ENTRIES:
        raise ImportFailure("Client archive contains too many entries")
    by_name = {}
    for entry in entries:
        safe_relative(entry.filename.rstrip("/"))
        if stat.S_ISLNK(entry.external_attr >> 16):
            raise ImportFailure(f"Archive link is not accepted: {entry.filename}")
        if entry.filename in by_name:
            raise ImportFailure(f"Duplicate archive entry: {entry.filename}")
        by_name[entry.filename] = entry
    missing = sorted(set(JAR_RESOURCES) - by_name.keys())
    if missing:
        raise ImportFailure("Missing required JAR resources: " + ", ".join(missing))
    selected = [by_name[name] for name in JAR_RESOURCES]
    for entry in selected:
        if entry.is_dir() or not 0 < entry.file_size <= MAX_FILE_BYTES:
            raise ImportFailure(f"Invalid JAR resource size or type: {entry.filename}")
    return selected


def inspect_external(root: Path) -> list[tuple[str, Path, int]]:
    check_ancestors(root)
    plain_path(root, directory=True)
    files = []
    missing = [name for name in REQUIRED_EXTERNAL if not (root / name).exists()]
    if missing:
        raise ImportFailure(
            "Missing required external resources: " + ", ".join(missing)
            + ". --resources must name the directory containing sound/, newsound/, "
            "streaming/, music/, and newmusic/."
        )
    visited = 0
    for group in RESOURCE_GROUPS:
        directory = root / group
        plain_path(directory, directory=True)
        pending = [directory]
        group_files = 0
        while pending:
            current = pending.pop()
            plain_path(current, directory=True)
            with os.scandir(current) as iterator:
                children = []
                for child in iterator:
                    visited += 1
                    if visited > MAX_ENTRIES:
                        raise ImportFailure("External resources contain too many entries")
                    children.append(child)
            children.sort(key=lambda entry: entry.name)
            for child in children:
                path = Path(child.path)
                relative = path.relative_to(root).as_posix()
                safe_relative(relative)
                if child.is_dir(follow_symlinks=False):
                    plain_path(path, directory=True)
                    pending.append(path)
                    continue
                size = plain_path(path, directory=False).st_size
                if path.suffix.lower() not in (".ogg", ".mus"):
                    raise ImportFailure(f"Unsupported external resource extension: {relative}")
                if not 0 < size <= MAX_FILE_BYTES:
                    raise ImportFailure(f"External resource size outside allowed range: {relative}")
                files.append((relative, path, size))
                group_files += 1
        if not group_files:
            raise ImportFailure(f"External resource directory is empty: {directory}")
    return sorted(files)


def check_output(output: Path, jar: Path, resources: Path) -> None:
    check_ancestors(output.parent)
    if output.exists() or output.is_symlink():
        raise ImportFailure(f"Output already exists; choose a fresh local directory: {output}")
    # Refuse accidental publication paths inside this source checkout. Paths outside
    # the checkout remain useful for user-selected application data directories.
    checkout = Path(__file__).absolute().parents[2]
    try:
        relative = output.relative_to(checkout)
    except ValueError:
        pass
    else:
        if not relative.parts or relative.parts[0] not in (".local", "assets-local", "research-local"):
            raise ImportFailure("Inside this checkout, output must be under .local/, assets-local/, or research-local/")
    for source in (jar, resources):
        if output == source or output in source.parents or source in output.parents:
            raise ImportFailure("Output must be separate from the JAR and external resource inputs")


def imported_sdk_icons(path: Path, output: Path) -> tuple[dict[str, bytes], dict]:
    """Select verified import-only SDK data, never a runtime class path."""
    path = Path(os.path.abspath(path))
    check_ancestors(path.parent)
    plain_path(path, directory=False)
    if output == path or output in path.parents or path in output.parents:
        raise ImportFailure("Output must be separate from the SDK input")
    helper = Path(__file__).with_name("lwjgl_icons.py")
    spec = importlib.util.spec_from_file_location("asset_import_lwjgl_icons", helper)
    if spec is None or spec.loader is None:
        raise ImportFailure("Missing SDK icon import module")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        return module.read_sdk(path)
    except module.IconImportFailure as error:
        raise ImportFailure(str(error)) from error


def import_assets(jar: Path, resources: Path, output: Path, *, lwjgl_jar: Path | None = None) -> dict:
    jar = Path(os.path.abspath(jar))
    resources = Path(os.path.abspath(resources))
    output = Path(os.path.abspath(output))
    check_output(output, jar, resources)
    client_hash = verify_client_jar(jar)
    external = inspect_external(resources)
    sdk_files, sdk_provenance = ({}, None) if lwjgl_jar is None else imported_sdk_icons(lwjgl_jar, output)
    sdk_metadata = b"" if sdk_provenance is None else (json.dumps(sdk_provenance, indent=2) + "\n").encode("utf-8")
    with zipfile.ZipFile(jar) as archive:
        jar_entries = inspect_jar(archive)
        total = sum(entry.file_size for entry in jar_entries) + sum(item[2] for item in external)
        total += sum(len(data) for data in sdk_files.values()) + len(sdk_metadata)
        if total > MAX_TOTAL_BYTES:
            raise ImportFailure(f"Import exceeds {MAX_TOTAL_BYTES} total bytes")
        output.parent.mkdir(parents=True, exist_ok=True)
        check_ancestors(output.parent)
        manifest = {
            "format": 1,
            "target": "Minecraft Beta 1.7.3 client",
            "client_sha256": client_hash,
            "external_validation": "availability floor; canonical audio parity unverified",
            "files": [],
        }
        with tempfile.TemporaryDirectory(prefix=".asset-import-", dir=output.parent) as temporary:
            stage = Path(temporary) / "assets"
            stage.mkdir()
            for entry in jar_entries:
                relative = "jar/" + entry.filename
                target = stage / safe_relative(relative)
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(entry) as source, target.open("xb") as destination:
                    digest = copy_bounded(source, destination, entry.file_size)
                manifest["files"].append({"path": relative, "bytes": entry.file_size, "sha256": digest})
            for name, path, size in external:
                # Recheck inputs just before opening; do not follow replaced links.
                check_ancestors(path.parent)
                if plain_path(path, directory=False).st_size != size:
                    raise ImportFailure(f"External resource changed during import: {name}")
                relative = "resources/" + name
                target = stage / safe_relative(relative)
                target.parent.mkdir(parents=True, exist_ok=True)
                with path.open("rb") as source, target.open("xb") as destination:
                    if path.suffix.lower() == ".ogg":
                        if source.read(4) != b"OggS":
                            raise ImportFailure(f"External .ogg lacks an Ogg header: {name}")
                        source.seek(0)
                    digest = copy_bounded(source, destination, size)
                manifest["files"].append({"path": relative, "bytes": size, "sha256": digest})
            if sdk_provenance is not None:
                manifest["sdk_icons"] = sdk_provenance
                for relative, data in (*sdk_files.items(), ("sdk/provenance.json", sdk_metadata)):
                    target = stage / safe_relative(relative)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with target.open("xb") as destination:
                        written = destination.write(data)
                        if written != len(data):
                            raise ImportFailure("Short SDK asset write")
                    manifest["files"].append({"path": relative, "bytes": len(data),
                                              "sha256": hashlib.sha256(data).hexdigest()})
            (stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            check_output(output, jar, resources)
            stage.rename(output)
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True, help="Verified local b1.7.3.jar")
    parser.add_argument("--resources", type=Path, required=True, help="Local root containing sound/, newsound/, music/, newmusic/, streaming/")
    parser.add_argument("--output", type=Path, required=True, help="Fresh local asset directory; never overwrites an existing path")
    parser.add_argument("--lwjgl-jar", type=Path,
                        help="Optional verified SDK2.9.0 input, used only during asset import for both real window icons")
    args = parser.parse_args(argv)
    try:
        manifest = import_assets(args.jar, args.resources, args.output, lwjgl_jar=args.lwjgl_jar)
    except (ImportFailure, OSError, zipfile.BadZipFile, RuntimeError) as error:
        print(f"Asset import unavailable: {error}", file=sys.stderr)
        return 1
    print(f"Imported {len(manifest['files'])} resources into {args.output}")
    print("External audio validation is an availability floor; canonical audio parity is unverified.")
    print("The native client reads the imported files without a JAR, launcher, or Java installation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
