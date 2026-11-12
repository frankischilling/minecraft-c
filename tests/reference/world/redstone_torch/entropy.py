"""Own-process continuation audit. Never stores or supplies a game RNG seed."""
import importlib.util
import math
import re
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[4]
_spec = importlib.util.spec_from_file_location("db_torch_entropy_parent",
    Path(__file__).with_name("rng.py"))
E = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(E)
require = E.require

# Successor-only input gate and accounting. Normal algorithm below is unchanged.
import json
from importlib.util import spec_from_file_location, module_from_spec
_bspec=spec_from_file_location("db_unpublished_witness",Path(__file__).with_name("witness.py"))
B=module_from_spec(_bspec);_bspec.loader.exec_module(B)
_DECLARED=json.loads(Path(__file__).with_name("declared-inputs.json").read_text())["cohorts"]

def bounded_only(row, raw):
    if row["cohort"]!="lifecycle" or row["index"] not in (58,59,62,63):
        return None
    require(type(row["index"]) is int, "exact bounded input index")
    return B.bounded_witness(row,raw,_DECLARED["lifecycle"][row["index"]])



def cache(before, after):
    require(E.same_cache(before, after), "Source nextFloat/nextDouble Gaussian cache")


def actual_bits(value):
    return struct.unpack(">d", bytes.fromhex(value))[0]


def audit_cohort(rows, own):
    require(len(rows) == len(own), "complete own-entropy rows")
    previous_worlds, previous_independent, previous_math = {}, None, None
    totals = dict(rows=0, drops=0, fizz=0, particles=0, cold_math_prestate_unobserved=0)
    inferred = []
    for index, (row, raw) in enumerate(zip(rows, own)):
        require(row["index"] == raw["index"] == index and row["name"] == raw["name"], "entropy row pairing")
        before, after = raw["owners_before"], raw["owners_after"]
        wb = {r["role"]: r for r in before["worlds"]}
        wa = {r["role"]: r for r in after["worlds"]}
        require(set(wb) == set(wa), "all actual World Random owners")
        for role in wb:
            require(wb[role]["alias"] == wa[role]["alias"], "same actual World Random owner")
            if role in previous_worlds:
                require(previous_worlds[role] == wb[role], "World continuation across authored geometry")
        ib, ia = before["independent"], after["independent"]
        require(ib["alias"] == ia["alias"], "same one independent Random owner")
        if previous_independent is None:
            require(E.state(ib["state"]) == ((31 ^ E.MULTIPLIER) & E.MASK), "declared Random(31) constructor state")
        else:
            require(previous_independent == ib, "independent continuation across rows")
        role = row["world_role"]
        world_state, independent_state = wb[role]["state"], ib["state"]
        world_seed, independent_seed = E.state(world_state), E.state(independent_state)
        require(raw["mode"] == row["values"][11] and raw["supplied_alias"] ==
                (wb[role]["alias"] if raw["mode"] == 0 else ib["alias"] if raw["mode"] == 1 else 0),
                "actual supplied argument owner/alias/null")
        cache(world_state, wa[role]["state"]); cache(independent_state, ia["state"])
        if index != 0:
            require(previous_math == raw["math_before"], "observed Math continuation across rows")
        current_math = raw["math_before"]
        drops = 0
        for ordinal, event in enumerate(raw["events"]):
            require(event["ordinal"] == ordinal and event["world"] == role, "raw event order/World")
            kind = event["event"]
            if kind == "item":
                world_seed, draws = E.floats(world_seed, 4)
                spread = E.f32(.7); margin = E.f32(E.f32(1.0) - spread) * .5
                xyz = [E.double_bits(float(c) + (E.f32(v * spread) + margin))
                       for c, v in zip(row["values"][3:6], draws[1:])]
                require(event["position"] == xyz, "actual own-stream Torch drop origin")
                post = event["math_post"]; post_seed = E.state(post)
                predecessor = E.rewind(post_seed, 8)
                finish, values = E.doubles(predecessor, 4)
                require(finish == post_seed, "actual Item Math eight-step continuation")
                if current_math is None:
                    totals["cold_math_prestate_unobserved"] += 1
                else:
                    require(E.state(current_math) == predecessor, "actual Math predecessor")
                    cache(current_math, post)
                bob = values[0] * math.pi; bob = bob * 2.0
                require(event["bob"] == E.float_bits(bob) and event["yaw"] == E.float_bits(values[1] * 360.0),
                        "actual Item bob/yaw source formulas")
                scale, edge = E.f32(.2), E.f32(.1)
                require(event["motion"] == [E.double_bits(E.f32(values[2] * scale - edge)),
                        E.double_bits(scale), E.double_bits(E.f32(values[3] * scale - edge))],
                        "actual Item motion source formulas")
                E.state(event["entity_random"])
                require(event["entity_random"]["gaussian"] == "0000000000000000" and
                        event["entity_random"]["have"] is False, "default Entity Random cache only")
                current_math = post; drops += 1; totals["drops"] += 1
            elif kind == "sound":
                require(event["name"] == "random.fizz", "reached DB fizz sound")
                world_seed, values = E.floats(world_seed, 2)
                pitch = E.f32(E.f32(2.6) + E.f32(E.f32(values[0] - values[1]) * E.f32(.8)))
                # Source order is verified again by Root from DB PCs132..151
                # before execution release; raw fields are always retained.
                require(event["pitch"] == E.float_bits(pitch), "actual own-stream fizz pitch")
                require(event["volume"] == E.float_bits(.5), "actual fizz volume")
                require(event["position"] == [E.double_bits(E.f32(E.f32(c) + E.f32(.5)))
                        for c in row["values"][3:6]], "actual fizz origin")
                totals["fizz"] += 1
            elif kind == "particle":
                require(event["name"] == "smoke" and raw["mode"] in (0, 1), "reached supplied stream")
                if raw["mode"] == 0:
                    world_seed, values = E.doubles(world_seed, 3)
                else:
                    independent_seed, values = E.doubles(independent_seed, 3)
                xyz = [E.double_bits(float(c) + v * .6 + .2)
                       for c, v in zip(row["values"][3:6], values)]
                require(event["position"] == xyz and event["motion"] == [E.double_bits(0.0)] * 3,
                        "actual supplied-stream smoke source arithmetic")
                totals["particles"] += 1
            else:
                raise ValueError("unplanned raw entropy event")
            states = event["states"]
            require(E.state(states["world"]) == world_seed and E.state(states["independent"]) == independent_seed,
                    "actual event-boundary continuations")
            cache(world_state, states["world"]); cache(independent_state, states["independent"])
        witness = bounded_only(row,raw)
        if witness is not None:
            # Advance only expected local cursors; never replace Source observations.
            world_seed, unused_world_draws = E.floats(world_seed,4)
            require(current_math is not None,"bounded unpublished path requires observed Math predecessor")
            expected_math, unused_math_draws = E.doubles(E.state(current_math),4)
            require(expected_math==E.state(raw["math_after"]),"bounded expected Math continuation")
            cache(current_math,raw["math_after"])
            current_math=raw["math_after"]
            inferred.append(witness)
        require(len(row["drops"]) == drops, "logical/raw Item pairing")
        require(E.state(wa[role]["state"]) == world_seed and E.state(ia["state"]) == independent_seed,
                "complete reached stream continuations")
        for other in wb:
            if other != role: require(wb[other] == wa[other], "unreached World Random unchanged")
        require(raw["math_after"] == current_math,
                "UNQUALIFIED_UNPUBLISHED_MATH_CONSTRUCTION_OR_UNPLANNED_MATH_CONSUMPTION: raw states retained")
        require(all(type(raw[k]) is str and re.fullmatch("[0-9a-f]{16}",raw[k])
                    for k in ("seed_uniquifier_before","seed_uniquifier_after")),
                "actual global default-constructor counter receipts")
        previous_worlds = wa; previous_independent = ia; previous_math = current_math
        totals["rows"] += 1
    return dict(totals, inferred_unpublished_HL=len(inferred), unpublished_accounting=inferred)
