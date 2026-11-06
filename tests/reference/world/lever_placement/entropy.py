"""Own-process source-formula audit; never supplies or stores a game RNG seed."""

import math
import re
import struct

MASK = (1 << 48) - 1
MULTIPLIER = 0x5DEECE66D
INVERSE = pow(MULTIPLIER, -1, 1 << 48)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def double_bits(value):
    return struct.pack(">d", value).hex()


def float_bits(value):
    return struct.pack(">f", value).hex()


def state(value):
    require(type(value) is dict and set(value) == {"seed", "gaussian", "have"}, "Random state schema")
    require(type(value["seed"]) is str and re.fullmatch("[0-9a-f]{12}", value["seed"]), "48-bit state")
    require(type(value["gaussian"]) is str and re.fullmatch("[0-9a-f]{16}", value["gaussian"]), "Gaussian bits")
    require(type(value["have"]) is bool, "Gaussian flag")
    return int(value["seed"], 16)


def step(seed, bits):
    updated = (seed * MULTIPLIER + 11) & MASK
    return updated, updated >> (48 - bits)


def floats(seed, count):
    values = []
    for _ in range(count):
        seed, bits = step(seed, 24)
        values.append(bits / (1 << 24))
    return seed, values


def doubles(seed, count):
    values = []
    for _ in range(count):
        seed, first = step(seed, 26)
        seed, second = step(seed, 27)
        values.append(((first << 27) + second) / (1 << 53))
    return seed, values


def rewind(seed, count):
    for _ in range(count):
        seed = ((seed - 11) * INVERSE) & MASK
    return seed


def same_cache(before, after):
    return before["gaussian"] == after["gaussian"] and before["have"] == after["have"]


def audit_entropy(torch, entropy):
    """Audit actual independent native/original states; no cross-process equality."""
    require(type(entropy) is dict and set(entropy) == {
        "index", "name", "world_before", "world_after", "math_before", "math_after", "items"}, "entropy schema")
    require(entropy["index"] == torch["index"] and entropy["name"] == torch["name"], "entropy row identity")
    before, after = entropy["world_before"], entropy["world_after"]
    seed = state(before)
    world_after = state(after)
    require(same_cache(before, after), "nextFloat preserves World Gaussian cache")
    require(type(entropy["items"]) is list and len(entropy["items"]) == len(torch["drops"]), "actual published Item inventory")
    require(torch["world_rng_unchanged"] == (before == after), "literal World Random equality receipt")
    floor_step = (torch["values"][0] == 4 and torch["values"][5] == 1
                  and not torch["values"][10] and torch["values"][8] == 1)
    if floor_step:
        seed, bits = step(seed, 31)
        orientation = 5 + ((2 * bits) >> 31)
        require(torch["center_after"] == [69, orientation + (torch["values"][4] & 8)],
                "own World nextInt2 placement orientation")
    previous_math = entropy["math_before"]
    if previous_math is not None:
        state(previous_math)
    for ordinal, (item, logical) in enumerate(zip(entropy["items"], torch["drops"])):
        require(type(item) is dict and set(item) == {
            "ordinal", "position", "motion", "yaw", "bob", "entity_random", "math_post"}, "Item entropy schema")
        require(item["ordinal"] == ordinal and logical[0] == ordinal, "actual Item ordinal")
        require(logical[1:] == ["hl", 69, 1, 0, 10, 0, 5], "ordinary Lever drop Stack/default fields")
        seed, draws = floats(seed, 4)
        # UU probability float, then three UU spawn floats. Keep fmul in f32,
        # widen before adding the separately widened margin and integer xyz.
        spread = f32(0.7)
        margin = f32(f32(1.0) - spread) * 0.5
        positions = []
        for coordinate, draw in zip(torch["values"][1:4], draws[1:]):
            scaled = f32(draw * spread)
            offset = scaled + margin
            positions.append(double_bits(float(coordinate) + offset))
        require(item["position"] == positions, "actual own-World-Random drop positions")
        math_post = item["math_post"]
        post_seed = state(math_post)
        reconstructed = rewind(post_seed, 8)
        final_seed, draws = doubles(reconstructed, 4)
        require(final_seed == post_seed, "own post-Math reconstruction")
        if previous_math is not None:
            require(state(previous_math) == reconstructed and same_cache(previous_math, math_post), "own Math continuation")
        # First predecessor is inferred backward from the observed poststate;
        # no initial seed/phase observation is claimed for that first Item.
        bob = draws[0] * math.pi
        bob = bob * 2.0
        require(item["bob"] == float_bits(bob), "actual constructor bob formula")
        require(item["yaw"] == float_bits(draws[1] * 360.0), "actual constructor yaw formula")
        scale, edge = f32(0.2), f32(0.1)
        vx = f32(draws[2] * scale - edge)
        vz = f32(draws[3] * scale - edge)
        require(item["motion"] == [double_bits(vx), double_bits(scale), double_bits(vz)], "actual constructor motion formulas")
        state(item["entity_random"])
        require(not item["entity_random"]["have"] and item["entity_random"]["gaussian"] == "0000000000000000",
                "actual default Entity Random cache; its entropy seed remains unpredicted")
        previous_math = math_post
    require(seed == world_after, "actual World Random four-float-per-drop continuation")
    if entropy["math_after"] is None:
        require(previous_math is None, "unobserved first Math state")
    else:
        state(entropy["math_after"])
        require(previous_math == entropy["math_after"], "post-call actual Math continuation")
    return {"drops": len(entropy["items"]), "world_float_steps": 4 * len(entropy["items"]),
            "math_lcg_steps": 8 * len(entropy["items"]), "world_int_steps": int(floor_step),
            "first_math_prestate_unobserved": entropy["math_before"] is None and bool(entropy["items"])}


def audit_cohort(rows, entropy_rows):
    require(len(rows) == len(entropy_rows), "complete entropy cohort")
    totals = {"drops": 0, "world_float_steps": 0, "math_lcg_steps": 0, "world_int_steps": 0, "first_math_prestate_unobserved": 0}
    previous = None
    for row, own in zip(rows, entropy_rows):
        if previous is not None:
            require(previous["world_after"] == own["world_before"], "World Random joins across declared profile setup")
            require(previous["math_after"] == own["math_before"], "observed Math joins across declared profile setup")
        result = audit_entropy(row, own)
        for key in totals:
            totals[key] += result[key]
        previous = own
    return totals
