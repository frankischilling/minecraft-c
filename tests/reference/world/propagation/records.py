"""Exact comparison of newly generated original/native observation transports."""
import hashlib
import json
import struct

FIELDS = (("Blocks", 0, 32768), ("Metadata", 32768, 49152),
          ("SkyLight", 49152, 65536), ("BlockLight", 65536, 81920),
          ("HeightMap", 81920, 82176))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def chunks(path):
    data = path.read_bytes()
    magic, count = struct.unpack_from(">II", data)
    require(magic == 0x4C495431 and len(data) == 8 + count * 82184,
            f"invalid Chunk transport: {path}")
    result = {}
    for offset in range(8, len(data), 82184):
        key = struct.unpack_from(">ii", data, offset)
        require(key not in result, f"duplicate concrete Chunk coordinate: {key}")
        result[key] = data[offset + 8:offset + 82184]
    return result


def stdout(path):
    objects, diagnostics = [], []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("{"):
            objects.append(json.loads(line))
        elif line:
            diagnostics.append(line)
    return objects, diagnostics


def scheduler(path):
    data = path.read_bytes()
    magic, f_count, e_count = struct.unpack_from(">III", data)
    require(magic == 0x53434831 and len(data) == 12 + 32 * f_count + 36 * e_count,
            f"invalid scheduler transport: {path}")
    membership = [struct.unpack_from(">iiiiqq", data, 12 + 32 * i) for i in range(f_count)]
    ordered = [struct.unpack_from(">iiiiqqI", data, 12 + 32 * f_count + 36 * i)
               for i in range(e_count)]
    require(len({x[:4] for x in membership}) == f_count, "duplicate F coordinate/Block key")
    require(len({x[5] for x in membership}) == f_count, "duplicate retained sequence")
    require(all(x[-1] == 1 for x in ordered), "E record is not the actual same owner in F")
    require(set(membership) == {x[:6] for x in ordered}, "F/E full record inventories differ")
    require(membership == sorted(membership), "F transport is not canonical")
    require([x[4:6] for x in ordered] == sorted(x[4:6] for x in ordered),
            "E structural order differs from current due-time/sequence order")
    return {"F_records": f_count, "E_records": e_count,
            "actual_same_F_owner_aliases": e_count}


def queues(path):
    data = path.read_bytes()
    require(struct.unpack_from(">I", data)[0] == 0x51433031, "invalid queue transport")
    offset, events, regions = 4, 0, 0
    while offset < len(data):
        event, phase, point, call, count = struct.unpack_from(">IIIII", data, offset)
        require(event == events, "nonsequential queue event")
        require(phase in (0, 1, 10, 11, 12), "unknown queue event phase")
        require(point <= 289 and call <= 10000, "queue event exceeds fixture bounds")
        offset += 20 + count * 28
        events += 1
        regions += count
    require(offset == len(data), "truncated ordered queue records")
    return {"events": events, "ordered_region_observations": regions}


def compare(original, native, profile):
    original_objects, original_diagnostics = stdout(original / "stdout.log")
    native_objects, native_diagnostics = stdout(native / "stdout.log")
    require(original_diagnostics == native_diagnostics, "original/native diagnostics differ")
    a = [x for x in original_objects if "event" in x]
    b = [x for x in native_objects if "event" in x]
    for index, (left, right) in enumerate(zip(a, b)):
        require(left == right, f"first boundary difference at event {index}: {left!r} != {right!r}")
    require(len(a) == len(b), f"event counts differ: {len(a)} != {len(b)}")
    require(bool(a) and a[-1]["phase"] == "complete", "missing completed original profile")
    expected_points = 0 if profile == "view" else ((2 * 128 // 16) + 1) ** 2
    require(a[-1]["point"] == expected_points and a[-1]["C"] == 0 and a[-1]["M"] == 0,
            "profile did not finish its acquisition/drain grid")
    inputs_a = next(x for x in original_objects if x.get("inputs"))
    inputs_b = next(x for x in native_objects if x.get("inputs"))
    for key in ("world_seed", "controlled_world_random_seed", "spawn"):
        require(inputs_a[key] == inputs_b[key], f"fixture input differs: {key}")
    origin = next(x for x in original_objects if x.get("runtime_origin"))
    abi = next(x for x in native_objects if x.get("native_abi"))
    require(origin["client_jar_sha256"] ==
            "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
            "actual fd runtime origin does not match the client pin")
    qa = queues(original / "data/queues.bin")
    qb = queues(native / "data/queues.bin")
    require(qa == qb and (original / "data/queues.bin").read_bytes() ==
            (native / "data/queues.bin").read_bytes(), "ordered C records differ")
    require(qa["events"] == len(a), "queue/state event inventories differ")
    array_results = {}
    phases = ["constructed.chunks"]
    if profile != "fresh":
        phases.extend(("view-undrained.chunks", "view-drained.chunks"))
    phases.append("final.chunks")
    for phase in phases:
        left = chunks(original / "data" / phase)
        right = chunks(native / "data" / phase)
        require(left.keys() == right.keys(), f"{phase}: concrete Chunk coordinates differ")
        for key in left:
            for name, start, end in FIELDS:
                require(left[key][start:end] == right[key][start:end],
                        f"{phase}: {name} differs at concrete Chunk {key}")
        array_results[phase] = {"concrete_chunks": len(left), "all_five_raw_arrays_equal": True}
    sa = scheduler(original / "data/scheduler.bin")
    sb = scheduler(native / "data/scheduler.bin")
    require(sa == sb and (original / "data/scheduler.bin").read_bytes() ==
            (native / "data/scheduler.bin").read_bytes(), "full F/E records/order/aliases differ")
    return {"status": "PASS", "profile": profile, "grid_points": expected_points,
            "grid_drain_calls": sum(x["phase"] == "drain" and x["point"] > 0 for x in a),
            "view_drain_calls": sum(x["phase"] == "drain" and x["point"] == 0 for x in a),
            "events": len(a), "ordered_C": qa, "arrays": array_results, "scheduler": sa,
            "all_boundary_Random_states_equal": True, "original_runtime_origin": origin,
            "native_ABI_diagnostic": abi, "native_caller_policy": inputs_b["native_caller_policy"]}
