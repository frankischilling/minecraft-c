"""File-only authored-input conversion. Does not import or launch the producer.

The codec bytes are stimulus admission data, never expected settings results.
The native direct Writer must reproduce them before each GameSettings call;
the later independent comparator must also join original input-control bytes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

CATALOG_SHA = "4312bd0190cc2c8fdef4ad32c1ad32e8f4fc8c57ac12f7d2b9847fe7c135db86"
CODECS = {"UTF-8": "utf-8", "windows-1252": "cp1252",
          "ISO-8859-1": "iso-8859-1", "US-ASCII": "ascii"}


def pin(raw: bytes) -> dict:
    return {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def unique(pairs: list) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate authored JSON key: {key}")
        result[key] = value
    return result


def control(value: dict, codec: str, reload: bool) -> tuple[str, dict]:
    kind = value.get("kind")
    text = b""
    payload = b""
    authored_utf8 = b""
    if kind == "default":
        if set(value) != {"kind", "text"} or not isinstance(value["text"], str):
            raise ValueError("authored default text schema")
        authored_utf8 = value["text"].encode("utf-8", errors="strict")
        text = value["text"].encode("utf-16-be", errors="strict")
        payload = value["text"].encode(codec, errors="replace")
    elif kind == "raw":
        if set(value) == {"kind", "hex"}:
            if not isinstance(value["hex"], str) or len(value["hex"]) % 2:
                raise ValueError("authored raw hex shape")
            payload = bytes.fromhex(value["hex"])
            if payload.hex() != value["hex"]:
                raise ValueError("canonical authored raw hex")
        elif set(value) == {"kind", "utf8"} and isinstance(value["utf8"], str):
            payload = value["utf8"].encode("utf-8", errors="strict")
        else:
            raise ValueError("authored raw schema")
    elif kind in ("absent", "directory") or (reload and kind == "keep"):
        if set(value) != {"kind"}:
            raise ValueError("no absent/directory/keep payload")
    else:
        raise ValueError("authored input kind")
    if len(authored_utf8) > 65536 or len(payload) > 65536 or len(text) // 2 > 65536:
        raise ValueError("complete original input bound")
    line = "\t".join((kind, text.hex() or "-", payload.hex() or "-"))
    provenance = {"kind": kind, "authored_utf8": {**pin(authored_utf8), "hex": authored_utf8.hex()},
                  "authored_utf16_be": {**pin(text), "hex": text.hex()},
                  "declared_stimulus": {**pin(payload), "hex": payload.hex()}}
    return line, provenance


def main() -> int:
    if not __debug__:
        raise RuntimeError("plan materialization requires assertions enabled")
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--charset", choices=CODECS, required=True)
    parser.add_argument("--lines", choices=("LF", "CRLF"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = args.catalog.read_bytes()
    if len(raw) > 65536 or pin(raw)["sha256"] != CATALOG_SHA:
        raise ValueError("exact frozen authored catalog pin")
    catalog = json.loads(raw.decode("utf-8", errors="strict"), object_pairs_hook=unique)
    if set(catalog) != {"schema", "scope", "cases"} or catalog["schema"] != 1 or \
            catalog["scope"] != "authored-options-inputs-only" or len(catalog["cases"]) != 16:
        raise ValueError("complete frozen authored catalog schema")
    # Prepare all derived bytes before creating the fresh destination.
    records = []
    ids = set()
    for case in catalog["cases"]:
        if set(case) != {"id", "constructor", "initial", "reload"}:
            raise ValueError("authored case schema")
        name = case["id"]
        if not isinstance(name, str) or not 1 <= len(name) <= 48 or \
                not "a" <= name[0] <= "z" or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-" for c in name) or name in ids:
            raise ValueError("unique authored case id")
        ids.add(name)
        if case["constructor"] not in ("file", "noarg"):
            raise ValueError("authored constructor")
        initial, initial_info = control(case["initial"], CODECS[args.charset], False)
        reload, reload_info = control(case["reload"], CODECS[args.charset], True)
        if case["constructor"] == "noarg" and (case["initial"] != {"kind": "absent"} or case["reload"] != {"kind": "keep"}):
            raise ValueError("exact noarg null-File scenario")
        plan = ("MC_GAME_SETTINGS_INPUT_V1\n" + f"case\t{name}\nconstructor\t{case['constructor']}\n"
                f"charset\t{args.charset}\nlines\t{args.lines}\ninitial\t{initial}\nreload\t{reload}\n").encode("ascii")
        if len(plan) > 1048576:
            raise ValueError("native complete plan bound")
        records.append((name, plan, initial_info, reload_info))
    args.output.mkdir(parents=False, exist_ok=False)
    (args.output / "catalog.json").write_bytes(raw)
    manifest = {"schema": 1, "scope": "catalog-only native GameSettings stimuli",
                "catalog": {"path": str(args.catalog.resolve()), **pin(raw)},
                "charset": args.charset, "python_codec": CODECS[args.charset], "lines": args.lines,
                "expected_settings_state_or_result_inputs": False, "producer_imported_or_launched": False,
                "original_input_bytes_join_still_required": True, "plans": []}
    for name, plan, initial, reload in records:
        path = args.output / (name + ".plan")
        with path.open("xb") as stream:
            stream.write(plan)
        if path.read_bytes() != plan:
            raise ValueError("written plan bytes changed")
        manifest["plans"].append({"id": name, "path": path.name, **pin(plan),
                                  "initial": initial, "reload": reload})
    if args.catalog.read_bytes() != raw or (args.output / "catalog.json").read_bytes() != raw:
        raise ValueError("consumed/copied catalog before-after byte join")
    with (args.output / "inputs.json").open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print("Prepared16 input-only GameSettings plans; no producer execution")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
