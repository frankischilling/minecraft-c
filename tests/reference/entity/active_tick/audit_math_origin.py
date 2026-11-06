"""Audit separately supplied source bytes; upstream source is not redistributed."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def audit(netlib, jdk17):
    pins = json.loads((Path(__file__).parent / "math-origin.json").read_text())
    tokens = {}
    for name, path in (("netlib-e_atan2.c", netlib), ("jdk17-e_atan2.c", jdk17)):
        raw = Path(path).read_bytes()
        expected = pins[name]
        if len(raw) != expected["bytes"] or hashlib.sha256(raw).hexdigest() != expected["sha256"]:
            raise ValueError("upstream source differs: " + name)
        text = raw.decode("utf-8")
        text = text[text.index("#include"):].split("\n", 1)[1]
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        text = re.sub(r"\s+", "", text)
        # These are the two audited differences in the pinned kernels.
        text = text.replace("(((hx-0x3ff00000)|lx)==0)", "((hx-0x3ff00000|lx)==0)")
        text = text.replace("return-1.0*zero", "return-zero")
        tokens[name] = text
        expected_token = pins["kernel_audit"]["normalized_token_sha256"][name]
        if hashlib.sha256(text.encode()).hexdigest() != expected_token:
            raise ValueError("kernel token pin differs: " + name)
    if len(set(tokens.values())) != 1:
        raise ValueError("normalized atan2 kernels differ")
    return {"same_normalized_tokens": True, "source_sha256": {
        name: pins[name]["sha256"] for name in tokens}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--netlib-source", type=Path, required=True)
    parser.add_argument("--jdk17-source", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.netlib_source, args.jdk17_source), sort_keys=True))
