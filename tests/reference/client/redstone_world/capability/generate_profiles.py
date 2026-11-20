"""Author/check the frozen-plan ten capability inputs; launches no target."""
import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def products():
    rows = []
    for block in (93, 94):
        for metadata in range(4):
            rows.append((f"west-repeater-{block}-metadata-{metadata}",
                         [0, 55, 7, 0, -2147483648, 0, -1, block, metadata, 0, 0, 0, 0]))
    for name, block in (("pressure-plate", 70), ("stone", 1)):
        rows.append(("west-" + name, [0, 55, 7, 0, -2147483648, 0, -1, block, 0, 0, 0, 0, 0]))
    assert len(rows) == 10 and len({name for name, _ in rows}) == 10
    profiles = "".join(name + " " + " ".join(map(str, values)) + "\n"
                       for name, values in rows)
    declaration = {
        "schema": 1,
        "status": "AUTHORED_NOT_COMPILED_OR_CAPTURED",
        "scope": "Ten direct CV.g capability rows in a separate fresh World/NW cohort; not appended to prior56/vertical20 histories",
        "profiles": 10,
        "integer_fields": ["mode_direct_g_must_zero", "registered_center_id_must_55",
                           "center_metadata_must_7", "light_must_dark_zero",
                           "preserve_emission_must_INT_MIN", "bm_delta_must_zero",
                           "texture_override_must_negative_one", "west_same_level_block_id",
                           "west_metadata_nibble", "reserved_9_must_zero",
                           "reserved_10_must_zero", "reserved_11_must_zero", "reserved_12_must_zero"],
        "coordinates": {"center": [8, 64, 8], "west": [7, 64, 8], "center_support": [8, 63, 8]},
        "world_seed": 23,
        "sky_subtraction": 3,
        "canonical_chunks": [[0, 0], [0, 1], [1, 0], [1, 1]],
        "preparation": "Before each call clear Blocks/Data/SkyLight/BlockLight of all four genuine Chunks; leave HeightMap/other fields intact. Set Stone1 below center, center55 with metadata7, and the declared West ID/metadata. Overhead, all other same-level neighbors and all below/above-neighbor cells remain air.",
        "groups": {"repeater_93_94_x_metadata_0_1_2_3": 8, "pressure_plate_70": 1, "stone_1": 1},
        "declaring_capability_owners": {"93": "wo.f()Z", "94": "wo.f()Z",
                                         "70": "bv.f()Z", "1": "uu.f()Z (real eo receiver)"},
        "jj_observation": "No target JJ getter, warmup or field authoring. Read-only JDI labels the actual JJ.b ArrayReference only at its real source AccessWatchpoint; records its actual elements, actual class state at first CV.g entry, and actual <clinit> entry/return if reached during the observed source call. Code pins include unchanged JJ.<clinit>176 bytes.",
        "bounds": "Preserve actual SM55 constructor bounds and bm; no bound setter",
        "mutable_inputs": ["Chunk block/metadata/SkyLight/BlockLight bytes",
                           "CV override=-1", "World sky subtraction f=3",
                           "LM.a=false query flag before each invocation"],
        "untouched": ["original JAR/classes/method bodies", "all Random/Math streams",
                      "all original static/final field references and table contents",
                      "registered Block fields", "NW constructor backing/references",
                      "Source clocks/phases/input", "World queues"],
        "nw": "One actual begin and translation(-.25,.5,-.75); real2097152-word backing, full own-prior non-emitted-word audit, no draw/backing/batch repair",
        "remaining_frozen_plan_cases": {"successful_signed_float_edges": 8,
                                         "nullable_failure_controls": 6},
        "remaining_total": 14,
        "ordinary_target_timeout_seconds": 90,
        "jdi_internal_timeout_seconds": 150,
        "jdi_outer_subprocess_timeout_seconds": 180,
        "jdi_event_limit": 20000,
        "jdi_journal_byte_limit": 16777216,
        "expected_source_values": "NONE: actual query order/results, JJ values, colors, return/exception and packed vertices are observations",
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
    print("PASS: 10 authored capability rows; no original/native execution")


if __name__ == "__main__":
    main()
