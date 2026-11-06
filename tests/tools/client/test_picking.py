"""Compare a prepared native PX body with unchanged-JAR observations."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zlib

RESOURCES = {
    "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                       b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
    "lang/stats_US.lang": b"# Authored statistics resource\n",
    "achievement/map.txt": b"",
}
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
FIELDS = ("partial_bits", "controller", "reach_bits", "camera_null", "world_null",
          "actors", "app_y", "px_m", "same_hit_as_before", "same_m_as_before",
          "nearby_order", "bt_cursor_before", "bt_cursor_after", "bt_pool_size", "vectors")
CONTROLS = ["source-null-px", "source-null-app-retained-call", "source-null-controller",
            "source-null-ray-world", "native-null-math", "owned-source-field-replacement",
            "foreign-px-hold", "retained-result-pool-and-world"]


def tag(kind, name, payload):
    encoded = name.encode("utf-8")
    return bytes([kind]) + struct.pack(">H", len(encoded)) + encoded + payload


def chunk(x, z):
    fields = [tag(3, "xPos", struct.pack(">i", x)), tag(3, "zPos", struct.pack(">i", z)),
              tag(4, "LastUpdate", struct.pack(">q", 0))]
    for name, size in (("Blocks", 32768), ("Data", 16384), ("SkyLight", 16384),
                       ("BlockLight", 16384), ("HeightMap", 256)):
        fields.append(tag(7, name, struct.pack(">i", size) + bytes(size)))
    fields += [tag(1, "TerrainPopulated", b"\1"), tag(9, "Entities", b"\x0a\0\0\0\0"),
               tag(9, "TileEntities", b"\x0a\0\0\0\0")]
    return tag(10, "", tag(10, "Level", b"".join(fields) + b"\0") + b"\0")


def authored_world(path):
    region = path / "region"
    region.mkdir(parents=True)
    header = bytearray(8192)
    sectors = []
    for x in range(2):
        for z in range(2):
            payload = b"\2" + zlib.compress(chunk(x, z))
            record = struct.pack(">I", len(payload)) + payload
            if len(record) > 4096:
                raise AssertionError("finite authored Chunk exceeded one sector")
            location = (2 + len(sectors)) << 8 | 1
            struct.pack_into(">I", header, 4 * (x + 32 * z), location)
            sectors.append(record + bytes(4096 - len(record)))
    (region / "r.0.0.mcr").write_bytes(header + b"".join(sectors))


def observations(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = data["rows"]
    if data.get("completion", {}).get("jar_sha256", data.get("jar_sha256")) != JAR:
        raise AssertionError("original picker JAR provenance")
    excluded = [row for row in rows if row["case"] == "pj_m_without_entity_vf"]
    rows = [row for row in rows if row["case"] != "pj_m_without_entity_vf"]
    if len(rows) != 26 or len(excluded) != 1 or len({row["case"] for row in rows}) != 26:
        raise AssertionError("26 original OS rows; one explicit unrepresentable PJ exclusion")
    for source, expected in data.get("source_sha256", {}).items():
        if hashlib.sha256((path.parent / source).read_bytes()).hexdigest() != expected:
            raise AssertionError(f"original picker source changed: {source}")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--observations", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    expected = observations(args.observations.resolve(strict=True))
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="picker-", dir=args.work_dir.resolve()))
    for name, data in RESOURCES.items():
        target = root / "resources" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    for index in range(26):
        authored_world(root / "worlds" / f"case{index:02}")
    authored_world(root / "worlds" / "controls")
    command = [str(args.executable.resolve(strict=True)), str(root / "resources"), str(root / "worlds")]
    process = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, timeout=150, check=False)
    (root / "native.stdout").write_bytes(process.stdout)
    (root / "native.stderr").write_bytes(process.stderr)
    if len(process.stdout) > 1024 * 1024 or len(process.stderr) > 65536:
        raise AssertionError(f"picker capture bound; retained {root}")
    if process.returncode or process.stderr:
        raise AssertionError(f"native picker failed ({process.returncode}); captures {root}\n"
                             + process.stderr.decode(errors="replace"))
    lines = process.stdout.decode("ascii").splitlines()
    actual = [json.loads(line[5:]) for line in lines if line.startswith("PICK ")]
    if [line[8:] for line in lines if line.startswith("CONTROL ")] != CONTROLS:
        raise AssertionError(f"complete ownership/failure controls; captures {root}")
    if len(actual) != len(expected):
        raise AssertionError(f"picker output inventory {len(actual)} !=26; captures {root}")
    for original, native in zip(expected, actual):
        if original["case"] != native["case"]:
            raise AssertionError(f"picker case identity/order; captures {root}")
        for name in FIELDS:
            if original[name] != native[name]:
                raise AssertionError(f"picker {original['case']} field {name}\n"
                                     f"original: {original[name]}\nnative: {native[name]}\n"
                                     f"captures: {root}")
    count = re.fullmatch(r"Prepared PX picking: ([1-9][0-9]*) checks; 26 original OS cases "
                         r"\(PJ explicitly excluded\)", lines[-1])
    if count is None:
        raise AssertionError(f"picker final completion receipt; captures {root}")
    result = dict(status="PASS", cases=26, excluded_original_pj=1, checks=int(count.group(1)),
                  ownership_controls=CONTROLS,
                  command=command, captures=str(root),
                  scope="Prepared PX/App body; genuine GAME Worlds/Entities/OS. No PX/App "
                        "constructor/default-session/rendering or PJ parity claim.")
    (root / "report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
