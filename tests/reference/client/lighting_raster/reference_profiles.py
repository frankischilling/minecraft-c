"""Select exact legacy observations using the native context's measured driver."""

import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "reference-profiles.json"


def require(value, message):
    if not value:
        raise RuntimeError(message)


def load_references(cohort, cases, row_count, parse_rows):
    manifest_bytes = MANIFEST.read_bytes()
    manifest = json.loads(manifest_bytes)
    require(manifest["schema"] == 1 and manifest["profiles"], "captured legacy driver profiles required")
    references, paths, identifiers = [], [MANIFEST, Path(__file__).resolve()], set()
    loaded_hashes = {str(MANIFEST.resolve()): hashlib.sha256(manifest_bytes).hexdigest()}

    def load_data(entry, identity):
        path = (HERE / entry["path"]).resolve(strict=True)
        require(path.is_relative_to(HERE), "legacy fixture remains in the reference directory")
        data = path.read_bytes()
        require(hashlib.sha256(data).hexdigest() == entry["sha256"], "captured legacy fixture hash: " + identity)
        paths.append(path)
        loaded_hashes[str(path)] = entry["sha256"]
        return path, json.loads(data)

    for profile in manifest["profiles"]:
        require(profile["id"] not in identifiers, "unique legacy driver profile identity")
        identifiers.add(profile["id"])
        entry = profile["fixtures"][cohort]
        path, fixture = load_data(entry, profile["id"])
        if cases is None:
            require(parse_rows("\n".join(fixture["rows"])) == fixture["rows"],
                    "complete original driver observation inventory")
        else:
            require(fixture["indices"] == list(range(len(cases))), "complete original driver fixture inventory required")
            require(len(fixture["rows"]) == sum(case["frames"] * row_count for case in cases),
                    "every original driver pixel and state row required")
            offset = 0
            for case in cases:
                selected = fixture["rows"][offset:offset + case["frames"] * row_count]
                require(parse_rows("\n".join(selected), case) == selected, "complete original driver profile rows")
                offset += len(selected)
        reference = {"profile": profile, "fixture": fixture, "path": path, "sha256": entry["sha256"]}
        if "observations" in entry:
            reference["observations_path"], reference["observations"] = load_data(entry["observations"], profile["id"])
            reference["observations_sha256"] = entry["observations"]["sha256"]
        references.append(reference)
    identities = [(reference["profile"]["native_driver"]["vendor"],
                   reference["profile"]["native_driver"]["renderer"],
                   reference["profile"]["native_driver"]["version"]) for reference in references]
    require(len(identities) == len(set(identities)), "unambiguous captured legacy driver identities")
    return references, paths, loaded_hashes


def select_reference(references, text):
    drivers = [line for line in text.splitlines() if line.startswith("RASTER_NATIVE_DRIVER ")]
    formats = [line for line in text.splitlines() if line.startswith("RASTER_NATIVE_FORMAT ")]
    require(len(drivers) == len(formats) == 1, "one actual native driver and framebuffer receipt required")
    driver = re.fullmatch(r"RASTER_NATIVE_DRIVER vendor=(.+) \| renderer=(.+) \| version=(.+)", drivers[0])
    format_row = re.fullmatch(r"RASTER_NATIVE_FORMAT rgba=(\d+),(\d+),(\d+),(\d+) depth=(\d+) "
                             r"stencil=(\d+) sample_buffers=(\d+) samples=(\d+) double_buffer=(\d+) dither=(\d+)",
                             formats[0])
    require(driver is not None and format_row is not None, "complete actual native driver/framebuffer fields")
    actual = (driver[1], driver[2], driver[3])
    matches = [reference for reference in references if actual ==
               (reference["profile"]["native_driver"]["vendor"],
                reference["profile"]["native_driver"]["renderer"],
                reference["profile"]["native_driver"]["version"])]
    require(len(matches) == 1, "no captured legacy lighting profile for actual driver: " + drivers[0])
    reference = matches[0]
    framebuffer = reference["profile"]["framebuffer"]
    actual_format = tuple(int(value) for value in format_row.groups())
    expected_format = (*framebuffer["rgba"], framebuffer["depth"], framebuffer["stencil"], 0, 0, 1, 0)
    require(actual_format == expected_format, "native framebuffer differs from captured legacy format")
    return reference, {"driver": drivers[0], "format": formats[0]}
