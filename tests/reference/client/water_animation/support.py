"""Authored trace protocol and reviewed bounded process capture."""
import importlib.util
from pathlib import Path
import re

CASES = (
    ("flat", 2, 0, ""), ("flat-color", 1, 0, ""), ("waves", 5, 7, ""),
    ("waves-color", 3, -2, ""), ("wrap", 1, 2147483647, ""),
    ("subnormal", 2, 0, ""), ("nan", 1, 0, ""), ("nan-color", 1, 0, ""),
    ("null-g", 1, 0, "g"), ("null-h", 1, 0, "h"), ("null-i", 1, 0, "i"),
    ("null-j", 1, 0, "j"), ("null-a", 1, 0, "a"), ("short-a2", 1, 0, "a2"),
    ("short-g0", 1, 0, "g0"), ("short-h1", 1, 0, "h1"), ("short-i0", 1, 0, "i0"),
    ("short-j0", 1, 0, "j0"), ("null-h-i", 1, 0, "hi"), ("short-j1", 1, 0, "j1"),
    ("clamp", 1, 0, ""), ("clamp-color", 1, 0, ""), ("signed-zero", 1, 0, ""))
MAX_TRACE_BYTES = 2097152


def require(condition, message):
    if not condition:
        raise ValueError(message)


def run(command, cwd, prefix, timeout=120, env=None):
    path = Path(__file__).resolve().parents[1] / "statistics/support.py"
    spec = importlib.util.spec_from_file_location("mc_water_capture", path)
    require(spec is not None and spec.loader is not None, "reviewed capture helper")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.run_bounded(command, cwd, prefix, timeout=timeout, env=env)
    require(result.capture_report.get("reader_errors") == {}, "capture reader errors")
    return result


def resources_at(root):
    for name, data in {
        "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
        "lang/stats_US.lang": b"# Authored water-animation resource\n",
        "achievement/map.txt": b"",
    }.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def advance(state, count):
    for _ in range(count):
        state = (state * 0x5deece66d + 11) & 0xffffffffffff
    return state


def inspect_trace(data, cases=CASES):
    require(0 < len(data) <= MAX_TRACE_BYTES, "trace extent")
    require(all(32 <= byte <= 126 or byte in (9, 10, 13) for byte in data) and
            b"\r" not in data.replace(b"\r\n", b"\n"), "trace ASCII/row terminators")
    lines = data.splitlines()  # Only physical platform row terminators differ.
    require(lines[-1:] == [b"END\t" + str(len(cases)).encode()], "terminal trace inventory")
    cursor = 0
    draws = 0
    for which, (name, frames, counter, fault) in enumerate(cases):
        state = ((0x123456789ab + which * 101) ^ 0x5deece66d) & 0xffffffffffff
        identities = {"a": 1, "g": 2, "h": 3, "i": 4, "j": 5}
        lengths = {"a": 1024, "g": 256, "h": 256, "i": 256, "j": 256}
        if fault == "hi":
            identities.update(h=0, i=0)
            lengths.update(h=-1, i=-1)
        elif fault:
            identities[fault[0]] = 0 if len(fault) == 1 else 6
            lengths[fault[0]] = -1 if len(fault) == 1 else int(fault[1])
        for frame in range(frames + 1):
            require(cursor + 6 <= len(lines) - 1, "truncated case rows")
            if frame:
                counter = (counter + 1) & 0xffffffff
                amount = 1 if fault == "j1" else 256 if not fault or fault.startswith("a") else 0
                draws += amount
                state = advance(state, amount * 2)
                if not fault or fault.startswith("a"):
                    identities["g"], identities["h"] = identities["h"], identities["g"]
                    lengths["g"], lengths["h"] = lengths["h"], lengths["g"]
            outcome = "SETUP" if frame == 0 else "OK" if not fault else "NULL" if len(fault) == 1 or fault == "hi" else "BOUNDS"
            expected = (f"CASE\t{name}\t{frame}\t{outcome}\t{counter & 0xffffffff:08x}"
                        f"\t{state:012x}\t0000000000000000\t0\t{int(name.endswith('-color'))}").encode()
            require(lines[cursor] == expected, "counter/shared holder/draw continuation: " + name + ":" + str(frame))
            cursor += 1
            for field in ("g", "h", "i", "j", "a"):
                family = "RGBA" if field == "a" else "ARRAY"
                prefix = f"{family}\t{name}\t{frame}\t" + ("" if field == "a" else field + "\t")
                prefix += f"{identities[field]}\t{lengths[field]}\t"
                row = lines[cursor]
                require(row.startswith(prefix.encode()), "actual array identity/length: " + name + ":" + field)
                raw = row[len(prefix):]
                require(len(raw) == max(0, lengths[field]) * (2 if field == "a" else 8) and
                    re.fullmatch(rb"[0-9a-f]*", raw) is not None, "raw array representation")
                cursor += 1
    require(cursor == len(lines) - 1, "trailing/duplicate trace rows")
    return {"rows": len(lines), "cases": len(cases), "frames": sum(x[1] for x in cases),
            "Math_random_calls": draws, "LCG_steps": draws * 2, "lines": lines}


def native_success(result):
    require(result.returncode == 0 and result.stderr == b"", "native process/diagnostics")
    lines = result.stdout.splitlines()
    require(lines[:-1] == [b"16 achievements", b"151 recipes"] * 3, "actual independent Registry banners")
    match = re.fullmatch(rb"Water animation: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    require(match is not None, "completed native checks")
    return int(match.group(1))
