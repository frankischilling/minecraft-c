"""Author/check twenty vertical-route data profiles; launches no target."""
import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def products():
    rows = []

    def add(name, mask, same=0, below=0, above=0, overhead=0, power=7):
        rows.append((name, [0, 55, power, 0, -2147483648, 0, -1,
                           mask, same, below, above, overhead, 0]))

    for name, mask in (("west", 1), ("east", 2), ("north", 4), ("south", 8)):
        add(name + "-lower-already-connected", mask, below=55)
        add(name + "-upper-dust-wall", mask, same=1, above=55)
        add(name + "-upper-dust-overhead-blocked", mask, same=1, above=55, overhead=1)
        add(name + "-upper-capability-not-dust", mask, same=1, above=75)
    for overhead, label in ((0, "open"), (1, "blocked")):
        for power in (0, 15):
            add(f"all-four-upper-dust-{label}-power-{power:02d}", 15,
                same=1, above=55, overhead=overhead, power=power)
    assert len(rows) == 20 and len({name for name, _ in rows}) == 20
    profiles = "".join(name + " " + " ".join(map(str, values)) + "\n"
                       for name, values in rows)
    declaration = {
        "schema": 1,
        "status": "AUTHORED_NOT_COMPILED_OR_CAPTURED",
        "scope": "Separate fresh twenty-row direct CV.g vertical cohort; not conditioned on the prior56 batch",
        "profiles": 20,
        "integer_fields": ["mode_direct_g_must_zero", "registered_block_id_must_55",
                           "center_metadata_nibble", "light_0dark_1sky15_2block15",
                           "preserve_emission_must_INT_MIN", "bm_delta_java_wrap_must_zero",
                           "texture_override_must_negative_one", "direction_mask_W1_E2_N4_S8",
                           "selected_same_level_block", "selected_below_block",
                           "selected_above_block", "center_overhead_block", "reserved_must_zero"],
        "coordinates": [8, 64, 8],
        "world_seed": 23,
        "sky_subtraction": 3,
        "canonical_chunks": [[0, 0], [0, 1], [1, 0], [1, 1]],
        "preparation": "Before each call clear Blocks/Data/SkyLight/BlockLight of all four actual Chunks; leave HeightMap/other owner state intact; Stone1 below center and center55+metadata; for each selected direction put the declared same/below/above IDs at y64/63/65; put declared center overhead at y65. Unselected cells remain air.",
        "groups": {"four_directions_x_four_routes": 16,
                   "all_four_walls_open_or_blocked_x_powers_0_15": 4},
        "routes": {
            "lower": "same-level air, below55, overhead air: initial flag becomes true; later independent same-level upper-support query must still be observed",
            "upper_dust": "same-level Stone1 and above55, overhead air: potential upward Dust strip",
            "blocked": "same-level Stone1 and above55, center overhead Stone1: overhead short circuits",
            "upper_capability": "same-level Stone1 and above registered RedstoneTorch75, overhead air: SM.e capability differs from late strict Dust identity",
        },
        "bounds": "Preserve actual registered SM constructor bounds; no Block bound setter",
        "mutable_inputs": ["Chunk block/metadata/SkyLight/BlockLight bytes",
                           "real CV texture override=-1", "World sky subtraction f=3",
                           "LM.a=false query flag immediately before each invocation"],
        "untouched": ["original JAR/class/method bodies", "all Random/Math streams",
                      "UU.s array identity AND every element", "registered Block55 bm and bounds",
                      "NW constructor backing/references", "Source clocks/phases/input",
                      "World light/scheduled queues"],
        "nw": "One actual begin and translation(-.25,.5,-.75), constructor capacity2097152; all non-emitted actual words own-prior audited per case, no backing/draw-state replacement or draw",
        "previous56": "Separate completed cohort remains immutable, not appended or relabeled; first56 original repeat is not native parity",
        "remaining_frozen_plan_cases": {"capability_repeater": 10,
                                         "successful_signed_float_edges": 8,
                                         "nullable_failure_controls": 6},
        "remaining_total": 24,
        "ordinary_target_timeout_seconds": 90,
        "jdi_internal_timeout_seconds": 150,
        "jdi_event_limit": 20000,
        "jdi_journal_byte_limit": 16777216,
        "expected_source_values": "NONE: return, exception, query short circuits, colors and vertices are captured unchanged-original observations, not generated here",
    }
    return {"profiles.txt": profiles,
            "declared-inputs.json": json.dumps(declaration, indent=2) + "\n"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for name, content in products().items():
        path = HERE / name
        wire = content.encode("ascii")
        if args.check:
            if path.read_bytes() != wire:
                raise AssertionError("authored input mismatch: " + name)
        else:
            path.write_bytes(wire)
    print("PASS: 20 authored vertical rows; no original/native execution")


if __name__ == "__main__":
    main()
