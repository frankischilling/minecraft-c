"""Author/check the frozen-plan eight signed-coordinate inputs; no target launch."""
import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
XS = [-16777217, -16777216, 16777215, 16777216]
CHUNKS = [[x, z] for x in (0, 1, -1048577, -1048576, 1048575, 1048576)
          for z in (0, 1)]


def products():
    rows = []
    for x in XS:
        for wall in (0, 1):
            name = "x-" + str(x) + ("-isolated" if wall == 0 else "-west-wall")
            rows.append((name, [0, 55, 7, 0, -2147483648, 0, -1, x, 64, 8, wall, 0, 0]))
    profiles = "".join(name + " " + " ".join(map(str, values)) + "\n" for name, values in rows)
    declaration = {
        "schema": 1,
        "status": "AUTHORED_NOT_COMPILED_OR_CAPTURED",
        "scope": "Eight direct CV.g signed-coordinate rows in one separate fresh World/NW cohort; not concatenated with prior cohorts",
        "profiles": 8,
        "integer_fields": ["mode_direct_g_must_zero", "registered_center_id_must_55",
                           "center_metadata_must_7", "light_must_dark_zero",
                           "preserve_emission_must_INT_MIN", "bm_delta_must_zero",
                           "texture_override_must_negative_one", "world_x",
                           "world_y_must_64", "world_z_must_8", "west_upright_0_or_1",
                           "reserved_11_must_zero", "reserved_12_must_zero"],
        "x_order": XS,
        "scene_order_per_x": ["isolated", "west-wall"],
        "world_seed": 23,
        "sky_subtraction": 3,
        "canonical_chunk_load_and_reset_order": CHUNKS,
        "requested_slots_vs_actual_owners": "12 saved Chunk/request slots; actual unchanged packed yy key + ok Map.get can alias slots. Preserve actual returned references, repeated-slot clearing, and request-slot voxel setup. Emit actual owner coords/first-equal-slot before STATIC. Current source predicts10 unique owners, slot10==slot0 and11==slot1; observed values are never authored.",
        "owner_receipt": "SIGNED_OWNERS TAB12 TABobservedUniqueCount TAB12 tokens slot,requestedX,requestedZ,actualLM.j,actualLM.k,firstEqualRequestSlot. Zero-based linear reference equality; owners.tsv is exact one-line ASCII-LF observation projection.",
        "cache_constructor_order": [[0, 0, 0, 31, 127, 31],
                                    [-16777232, 0, 0, -16777201, 127, 31],
                                    [16777200, 0, 0, 16777231, 127, 31]],
        "cache_selection": "negative x uses context1; positive x uses context2; context0 is genuine inherited base context with no direct profile",
        "saved_inputs": "Unchanged picking World producer supplies base4 canonical Chunks/level/session; local world_inputs.py adds8 empty populated far Chunks with exact signed NBT xPos/zPos, signed Region coordinates and masked local header index. No inherited file is replaced.",
        "preparation": "Before every call clear Blocks/Data/SkyLight/BlockLight of all12 returned request slots (possibly aliased genuine Chunks); leave HeightMap/other fields intact. Set Stone1 at(x,63,8), Dust55 metadata7 at(x,64,8); wall1 sets Stone1 at(x-1,64,8) and Dust55 metadata0 at(x-1,65,8). All other cells, including center-overhead, remain air. Resolve authored voxel to declared canonical Chunk with x>>4,z>>4 and local x&15,z&15.",
        "bounds": "All x are inside original horizontal World bounds; genuine EW arrays are constructed for matching far2x2 contexts. Preserve actual SM55 constructor bounds/bm, no bound setter.",
        "mutable_inputs": ["Declared canonical Chunk payload/light bytes", "CV override=-1",
                           "World sky subtraction f=3", "LM.a=false query flag before invocation"],
        "untouched": ["original JAR/classes/methods", "all Random/Math streams",
                      "Source clocks/phases/input", "all static/final references/table contents",
                      "registered Block fields", "NW constructor backing/references and cumulative batch",
                      "World queues and generated-provider behavior"],
        "nw": "One actual begin/translation(-.25,.5,-.75), actual2097152-word backing; own-prior audit of every non-emitted word, no draw/reset/backing replacement",
        "remaining_frozen_plan_cases": {"nullable_failure_controls": 6},
        "remaining_total": 6,
        "ordinary_target_timeout_seconds": 90,
        "jdi_internal_timeout_seconds": 150,
        "jdi_outer_subprocess_timeout_seconds": 180,
        "jdi_event_limit": 20000,
        "jdi_journal_byte_limit": 16777216,
        "expected_source_values": "NONE: return/exceptions/query order/colors/packed vertices are observations",
    }
    return {"profiles.txt": profiles, "declared-inputs.json": json.dumps(declaration, indent=2) + "\n"}


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
    print("PASS: eight authored signed rows; no original/native execution")


if __name__ == "__main__":
    main()
