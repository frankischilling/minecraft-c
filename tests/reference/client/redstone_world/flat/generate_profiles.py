"""Author/check the declared first-tranche inputs; never launches a target."""
import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def products():
    rows = []

    def add(name, mask=0, power=7, light=0, bm_delta=0, override=-1):
        rows.append((name, [0, 55, power, light, -2147483648, bm_delta,
                            override, mask, 0, 0, 0, 0, 0]))

    for mask in range(16):
        for power in (0, 7, 15):
            add(f"flat-mask-{mask:02d}-power-{power:02d}", mask, power)
    add("override-zero", mask=15, override=0)
    add("override-302", mask=15, override=302)
    add("override-int-max", mask=15, override=2147483647)
    add("override-negative-two", mask=15, override=-2)
    add("bm-negative-81", mask=15, bm_delta=-245)
    add("bm-int-max", mask=15, bm_delta=2147483483)
    add("skylight-15", mask=15, light=1)
    add("blocklight-15", mask=15, light=2)
    assert len(rows) == 56 and len({name for name, _ in rows}) == 56
    profiles = "".join(name + " " + " ".join(map(str, values)) + "\n"
                       for name, values in rows)
    declaration = {
        "schema": 1,
        "status": "AUTHORED_NOT_COMPILED_OR_CAPTURED",
        "scope": "First tranche: direct unchanged CV.g only; not full renderer qualification",
        "profiles": 56,
        "integer_fields": ["mode_direct_g_must_zero", "registered_block_id_must_55",
                           "metadata_nibble", "light_0dark_1sky15_2block15",
                           "preserve_emission_must_INT_MIN", "bm_delta_java_wrap",
                           "texture_override", "same_level_dust_mask_W1_E2_N4_S8",
                           "reserved_original_bounds_must_zero", "rotation_must_zero",
                           "flip_must_zero", "retained_ao_must_zero", "reserved_must_zero"],
        "coordinates": [8, 64, 8],
        "world_seed": 23,
        "sky_subtraction": 3,
        "canonical_chunks": [[0, 0], [0, 1], [1, 0], [1, 1]],
        "preparation": "Before every call clear Blocks/Data/SkyLight/BlockLight of all four actual Chunks; leave HeightMap/other owner state intact; set Stone1 below center, center55+metadata and selected same-level55 neighbors; light mode authors the 3x3x3 query neighborhood in Chunk0.",
        "bounds": "Preserve actual registered SM constructor bounds; no Block bound setter",
        "mutable_inputs": ["Chunk block/metadata/SkyLight/BlockLight bytes",
                           "registered Block55 bm (restored in finally)",
                           "real CV texture override", "World sky subtraction f=3",
                           "LM.a=false query flag immediately before each invocation"],
        "untouched": ["original JAR/class/method bodies", "all Random/Math streams",
                      "UU.s array identity AND every element", "NW constructor backing/references",
                      "Source clocks/phases/input", "World light/scheduled queues"],
        "nw": "One actual begin and actual translation(-.25,.5,-.75), constructor capacity2097152; own-prior audit of every non-emitted word per case, no backing/draw-state replacement or draw",
        "groups": {"floor_masks_x_three_powers": 48, "texture_and_light": 8},
        "remaining_frozen_plan_cases": {"vertical_routes": 20, "capability_repeater": 10,
                                         "successful_signed_float_edges": 8,
                                         "nullable_failure_controls": 6},
        "remaining_total": 44,
        "ordinary_target_timeout_seconds": 90,
        "jdi_internal_timeout_seconds": 150,
        "jdi_event_limit": 20000,
        "jdi_journal_byte_limit": 16777216,
        "expected_source_values": "NONE: returned results, exceptions, colors and packed vertices are observed from unchanged original calls, not generated here",
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
    print("PASS: 56 authored input rows; no original/native execution")


if __name__ == "__main__":
    main()
