"""Authored malformed trace controls; no game archive or native binary needed."""
import importlib.util
from pathlib import Path
import sys
sys.dont_write_bytecode = True


def main():
    path = Path(__file__).resolve().parents[2] / "reference/client/water_animation/support.py"
    spec = importlib.util.spec_from_file_location("water_driver_controls", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("water support unavailable")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    cases = (("flat", 1, 0, ""),)
    state = 0x123456789ab ^ 0x5deece66d
    rows = []
    for frame in (0, 1):
        if frame:
            state = support.advance(state, 512)
        rows.append(f"CASE\tflat\t{frame}\t{'OK' if frame else 'SETUP'}\t{frame:08x}\t{state:012x}\t0000000000000000\t0\t0".encode())
        for field, identity in zip("ghij", (2, 3, 4, 5) if not frame else (3, 2, 4, 5)):
            rows.append(f"ARRAY\tflat\t{frame}\t{field}\t{identity}\t256\t".encode() + b"00000000" * 256)
        rows.append(f"RGBA\tflat\t{frame}\t1\t1024\t".encode() + (b"2032ff92" if frame else b"00000000") * 256)
    rows.append(b"END\t1")
    valid = b"\n".join(rows) + b"\n"
    result = support.inspect_trace(valid, cases)
    support.require(result["Math_random_calls"] == 256 and result["LCG_steps"] == 512, "positive independent continuation")
    bad = {
        "NUL": valid.replace(b"CASE", b"CA\x00SE", 1),
        "non-ASCII": valid.replace(b"flat", b"fl\xfft", 1),
        "bare-CR": valid.replace(b"\n", b"\r", 1),
        "vertical-tab": valid.replace(b"\n", b"\v", 1),
        "missing-END": b"\n".join(rows[:-1]) + b"\n",
        "wrong-END": valid.replace(b"END\t1", b"END\t2"),
        "trailing-row": valid + b"extra\n",
        "wrong-counter": valid.replace(b"\t00000001\t", b"\t00000002\t", 1),
        "wrong-Random": valid.replace(f"{state:012x}".encode(), b"000000000000", 1),
        "wrong-Gaussian": valid.replace(b"0000000000000000\t0", b"0000000000000000\t1", 1),
        "wrong-mode": valid.replace(b"0000000000000000\t0\t0", b"0000000000000000\t0\t1", 1),
        "wrong-swap": valid.replace(b"ARRAY\tflat\t1\tg\t3", b"ARRAY\tflat\t1\tg\t2", 1),
        "wrong-length": valid.replace(b"\t256\t", b"\t255\t", 1),
        "nonhex-float": valid.replace(b"\t00000000", b"\tx0000000", 1),
        "truncated-color": valid.replace(b"2032ff92" * 256, b"2032ff92" * 255),
        "duplicate-array": b"\n".join(rows[:2] + rows[1:]) + b"\n",
    }
    for name, payload in bad.items():
        try:
            support.inspect_trace(payload, cases)
        except (ValueError, IndexError):
            pass
        else:
            raise RuntimeError("accepted malformed trace: " + name)
    support.require(support.inspect_trace(valid.replace(b"\n", b"\r\n"), cases)["rows"] == 13,
                    "actual platform CRLF rows")
    print("Water animation driver: 18 controls")


if __name__ == "__main__":
    main()
