"""Authored controls for the finite Lava trace and completion protocol."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import sys
sys.dont_write_bytecode = True


def main():
    path = Path(__file__).resolve().parents[2] / "reference/client/lava_animation/support.py"
    spec = importlib.util.spec_from_file_location("lava_driver_controls", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Lava support unavailable")
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    cases = (("flat", 1, ""),)
    state = 0x123456789ab ^ 0x5deece66d
    rows = [b"CTOR\tflat\t237\t0\t0\t1\t0"]
    for frame in (0, 1):
        if frame:
            for _ in range(512):
                state = (state * 0x5deece66d + 11) & 0xffffffffffff
        rows.append(f"CASE\tflat\t{frame}\t{'OK' if frame else 'SETUP'}\t{state:012x}\t0000000000000000\t0\t0".encode())
        for field, identity in zip("ghij", (2, 3, 4, 5) if not frame else (3, 2, 4, 5), strict=True):
            rows.append(f"ARRAY\tflat\t{frame}\t{field}\t{identity}\t256\t".encode() + b"00000000" * 256)
        rows.append(f"RGBA\tflat\t{frame}\t1\t1024\t".encode() + (b"9b0000ff" if frame else b"00000000") * 256)
    rows.append(b"END\t1")
    valid = b"\n".join(rows) + b"\n"
    parsed = support.inspect_trace(valid, cases)
    support.require(parsed["Math_random_calls"] == 256 and parsed["LCG_steps"] == 512 and
        parsed["rows"] == 14, "independent positive protocol")
    malformed = {
        "NUL": valid.replace(b"CASE", b"CA\x00SE", 1),
        "non-ASCII": valid.replace(b"flat", b"fl\xfft", 1),
        "bare-CR": valid.replace(b"\n", b"\r", 1),
        "vertical-tab": valid.replace(b"\n", b"\v", 1),
        "missing-END": b"\n".join(rows[:-1]) + b"\n",
        "wrong-END": valid.replace(b"END\t1", b"END\t2"),
        "trailing-row": valid + b"extra\n",
        "wrong-constructor-texture": valid.replace(b"\t237\t", b"\t205\t", 1),
        "wrong-constructor-default": valid.replace(b"237\t0\t0\t1\t0", b"237\t0\t0\t0\t0", 1),
        "wrong-Random": valid.replace(f"{state:012x}".encode(), b"000000000000", 1),
        "wrong-Gaussian": valid.replace(b"0000000000000000\t0", b"0000000000000000\t1", 1),
        "wrong-mode": valid.replace(b"0000000000000000\t0\t0", b"0000000000000000\t0\t1", 1),
        "wrong-swap": valid.replace(b"ARRAY\tflat\t1\tg\t3", b"ARRAY\tflat\t1\tg\t2", 1),
        "wrong-length": valid.replace(b"\t256\t", b"\t255\t", 1),
        "nonhex-float": valid.replace(b"\t00000000", b"\tx0000000", 1),
        "truncated-color": valid.replace(b"9b0000ff" * 256, b"9b0000ff" * 255),
        "duplicate-array": b"\n".join(rows[:2] + rows[1:]) + b"\n",
    }
    for name, payload in malformed.items():
        try:
            support.inspect_trace(payload, cases)
        except (ValueError, IndexError):
            pass
        else:
            raise RuntimeError("accepted malformed trace: " + name)
    support.require(support.inspect_trace(valid.replace(b"\n", b"\r\n"), cases)["rows"] == 14,
                    "physical CRLF records")
    output = b"16 achievements\n151 recipes\n" * 4 + b"Lava animation: 132659 checks\n"
    support.require(support.native_success(SimpleNamespace(returncode=0, stdout=output, stderr=b"")) == 132659,
                    "positive native completion")
    incomplete = (
        SimpleNamespace(returncode=False, stdout=output, stderr=b""),
        SimpleNamespace(returncode=1, stdout=output, stderr=b""),
        SimpleNamespace(returncode=0, stdout=output, stderr=b"diagnostic"),
        SimpleNamespace(returncode=0, stdout=output.replace(b"132659", b"0"), stderr=b""),
        SimpleNamespace(returncode=0, stdout=output + b"extra\n", stderr=b""),
    )
    for sample in incomplete:
        try:
            support.native_success(sample)
        except ValueError:
            pass
        else:
            raise RuntimeError("accepted incomplete native result")
    print("Lava animation driver: " + str(len(malformed) + len(incomplete) + 3) + " controls")


if __name__ == "__main__":
    main()
