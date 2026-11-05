"""Preserve selected native launch products; candidate SDL inputs are not link proof."""

import hashlib
import json
import os
from pathlib import Path
import re
import tempfile


class NativeProductCaptureError(RuntimeError):
    def __init__(self, receipt, manifest):
        super().__init__(receipt["error"])
        self.receipt = receipt
        self.manifest = manifest


def _fingerprint(stat):
    # Windows stat/fstat can expose different ctime semantics for the same file.
    return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns


def _measure(path):
    digest, size = hashlib.sha256(), 0
    with path.open("rb") as source:
        before = os.fstat(source.fileno())
        if _fingerprint(before) != _fingerprint(path.stat()):
            raise OSError(f"Native product replaced while opening: {path}")
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
            size += len(block)
        after, named_after = os.fstat(source.fileno()), path.stat()
    if (_fingerprint(before) != _fingerprint(after) or
            _fingerprint(after) != _fingerprint(named_after) or size != after.st_size):
        raise OSError(f"Native product changed while hashing: {path}")
    return {"sha256": digest.hexdigest(), "bytes": size}, _fingerprint(after)


def _copy(source, target):
    digest, size = hashlib.sha256(), 0
    with source.open("rb") as incoming, target.open("xb") as outgoing:
        for block in iter(lambda: incoming.read(1024 * 1024), b""):
            outgoing.write(block)
            digest.update(block)
            size += len(block)
    return {"sha256": digest.hexdigest(), "bytes": size}


def capture_native_products(command, evidence):
    """Capture before launch, or raise with a retained partial failure receipt.

    command[0] is an explicit filesystem path, resolved without PATH lookup.
    No process, assets, JAR or user-data discovery occurs. All sources stay read-only.
    """
    evidence = Path(evidence).resolve()
    destination = evidence / "native-products"
    receipt = {"schema": 1, "status": "CAPTURING", "products": [],
               "missing_optional_roles": [], "candidate_inputs_are_link_proof": False}
    manifest, owned = None, False
    try:
        evidence.mkdir(parents=True, exist_ok=True)
        if not isinstance(command, (list, tuple)) or not command:
            raise ValueError("Native command requires an explicit executable path")
        executable = Path(command[0]).resolve(strict=True)
        if not executable.is_file():
            raise ValueError("Native executable must be a file")
        receipt["command_executable"] = str(executable)
        adjacent = sorted(p for p in executable.parent.iterdir() if p.is_file() and
                          (p.suffix.lower() == ".dll" or re.search(r"\.so(?:\.[0-9]+)*$", p.name)))
        optional = {
            "adjacent_native_libraries": adjacent,
            "candidate_sdl_archive": [executable.parent / "_deps/mc_sdl3-build" / n
                                      for n in ("SDL3-static.lib", "libSDL3.a")],
            "candidate_sdl_source": [executable.parent / "mc-sdl3-native" / n
                                     for n in ("SDL_windowsevents.c", "SDL_x11events.c")],
        }
        selected = [("executable", executable)]
        for role, candidates in optional.items():
            present = [p for p in candidates if p.is_file()]
            if not present:
                receipt["missing_optional_roles"].append(role)
            selected.extend((role, p) for p in present)
        sources = [(role, path.name, path.resolve(strict=True)) for role, path in selected]
        if any(source.is_relative_to(destination) for _, _, source in sources):
            raise ValueError("Native source must be disjoint from capture destination")
        destination.mkdir(exist_ok=False)
        owned = True
        for role, name, source in sources:
            target = destination / role / name
            target.parent.mkdir(exist_ok=True)
            item = {"role": role, "source": str(source),
                    "artifact": target.relative_to(evidence).as_posix(), "status": "CAPTURING"}
            receipt["products"].append(item)
            item["before"], identity = _measure(source)
            item["copied"] = _copy(source, target)
            item["preserved"], _ = _measure(target)
            item["after"], after_identity = _measure(source)
            if (item["before"] != item["copied"] or item["before"] != item["preserved"] or
                    item["before"] != item["after"] or identity != after_identity):
                raise OSError(f"Native product changed during capture: {source}")
            item["status"] = "CAPTURED"
        receipt["status"] = "PASS"
    except Exception as error:
        receipt.update(status="FAIL", error=f"{type(error).__name__}: {error}")
        if receipt["products"] and receipt["products"][-1]["status"] == "CAPTURING":
            item = receipt["products"][-1]
            item["status"] = "FAILED"
            partial = evidence / item["artifact"]
            if partial.is_file():
                try:
                    item["partial"], _ = _measure(partial)
                except OSError as partial_error:
                    item["partial_error"] = str(partial_error)
    try:
        if owned:
            manifest = destination / "manifest.json"
            stream = manifest.open("x", encoding="utf-8")
        else:
            stream = tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", delete=False,
                                                  dir=evidence, prefix="native-products-error-",
                                                  suffix=".json")
            manifest = Path(stream.name)
        with stream:
            receipt["manifest"] = manifest.relative_to(evidence).as_posix()
            stream.write(json.dumps(receipt, indent=2) + "\n")
    except OSError as error:
        receipt.update(status="FAIL", error=receipt.get("error", "") +
                       f"; manifest {type(error).__name__}: {error}", manifest=None)
        manifest = None
    if receipt["status"] != "PASS":
        raise NativeProductCaptureError(receipt, manifest)
    return receipt
