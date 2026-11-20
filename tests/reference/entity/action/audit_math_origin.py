"""Verify separately supplied upstream sources; neither source is redistributed."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def audit(netlib, jdk17):
    pins = json.loads((Path(__file__).parent / "math-origin.json").read_text())
    tokens = {}
    for name, path in (("netlib-s_atan.c", netlib), ("jdk17-s_atan.c", jdk17)):
        raw = Path(path).read_bytes()
        expected = pins[name]
        if len(raw) != expected["bytes"] or hashlib.sha256(raw).hexdigest() != expected["sha256"]:
            raise ValueError("upstream source differs: " + name)
        text = raw.decode("utf-8")
        text = text[text.index("#include"):].split("\n", 1)[1]
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        tokens[name] = re.sub(r"\s+", "", text)
        if hashlib.sha256(tokens[name].encode()).hexdigest() != pins["kernel_audit"]["token_sha256"][name]:
            raise ValueError("kernel token pin differs: " + name)
    if len(set(tokens.values())) != 1:
        raise ValueError("upstream atan kernels differ")
    return {"same_post_include_tokens": True, "source_sha256": {
        name: pins[name]["sha256"] for name in tokens}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--netlib-source", type=Path, required=True)
    parser.add_argument("--jdk17-source", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.netlib_source, args.jdk17_source), sort_keys=True))
