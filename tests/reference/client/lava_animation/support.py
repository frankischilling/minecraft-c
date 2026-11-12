"""Finite Lava field protocol and actual bounded process capture."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

NAMES = ("flat", "flat-color", "waves", "waves-color", "waves-repeat", "subnormal", "nan", "nan-color",
         "null-g", "null-h", "null-i", "null-j", "null-a", "short-a2", "short-g0", "short-h1",
         "short-i0", "short-j0", "null-h-i", "short-j1", "clamp", "clamp-color", "signed-zero")
FRAMES = (2, 1, 5, 3, 1, 2) + (1,) * 17
FAULTS = ("", "", "", "", "", "", "", "", "g", "h", "i", "j", "a", "a2", "g0", "h1",
          "i0", "j0", "hi", "j1", "", "", "")

CASES = tuple(zip(NAMES, FRAMES, FAULTS, strict=True))
MAX_TRACE_BYTES = 2097152


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pin(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def inspect_trace(data, cases=None):
    cases = CASES if cases is None else cases
    require(0 < len(data) <= 2097152 and all(32 <= c <= 126 or c in (9, 10, 13) for c in data), "bounded ASCII trace")
    require(b"\r" not in data.replace(b"\r\n", b"\n"), "physical row terminators only")
    rows = data.splitlines()
    position = 0
    total_draws = 0
    for which, (name, frames, fault) in enumerate(cases):
        require(rows[position] == ("CTOR\t" + name + "\t237\t0\t0\t1\t0").encode(), "actual constructor fields")
        position += 1
        state = ((0x123456789ab + which * 101) ^ 0x5deece66d) & 0xffffffffffff
        identities = {"a": 1, "g": 2, "h": 3, "i": 4, "j": 5}
        lengths = {"a": 1024, "g": 256, "h": 256, "i": 256, "j": 256}
        if fault == "hi":
            identities.update(h=0, i=0)
            lengths.update(h=-1, i=-1)
        elif fault:
            identities[fault[0]] = 0 if len(fault) == 1 else 6
            lengths[fault[0]] = -1 if len(fault) == 1 else int(fault[1:])
        for frame in range(frames + 1):
            if frame:
                draws = 256 if not fault or fault.startswith("a") else 1 if fault in ("h1", "j1") else 0
                total_draws += draws
                for _ in range(draws * 2):
                    state = (state * 0x5deece66d + 11) & 0xffffffffffff
                if not fault or fault.startswith("a"):
                    identities["g"], identities["h"] = identities["h"], identities["g"]
                    lengths["g"], lengths["h"] = lengths["h"], lengths["g"]
            outcome = "SETUP" if frame == 0 else "OK" if not fault else "NULL" if len(fault) == 1 or fault == "hi" else "BOUNDS"
            expected = (f"CASE\t{name}\t{frame}\t{outcome}\t{state:012x}\t0000000000000000\t0\t{int(name.endswith('-color'))}").encode()
            require(rows[position] == expected, "actual outcome/Math state: " + name + ":" + str(frame))
            position += 1
            for field in ("g", "h", "i", "j", "a"):
                prefix = ("RGBA\t" if field == "a" else "ARRAY\t") + name + "\t" + str(frame) + "\t"
                if field != "a":
                    prefix += field + "\t"
                prefix += str(identities[field]) + "\t" + str(lengths[field]) + "\t"
                row = rows[position]
                require(row.startswith(prefix.encode()), "actual array identity/extent: " + name + ":" + field)
                values = row[len(prefix):]
                require(len(values) == max(0, lengths[field]) * (2 if field == "a" else 8) and
                        re.fullmatch(rb"[0-9a-f]*", values) is not None, "raw array representation")
                position += 1
    require(rows[position:] == [("END\t" + str(len(cases))).encode()], "exact terminal trace inventory")
    return {"rows": len(rows), "cases": len(cases), "frames": sum(case[1] for case in cases),
            "Math_random_calls": total_draws, "LCG_steps": total_draws * 2, "lines": rows}


def run(command, cwd, prefix, timeout=120, env=None):
    helper = Path(__file__).resolve().parents[1] / "statistics/support.py"
    spec = importlib.util.spec_from_file_location("lava_bounded_capture", helper)
    require(spec is not None and spec.loader is not None, "reviewed process helper")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    executable = Path(command[0]).resolve(strict=True)
    binary_before = pin(executable)
    result = module.run_bounded(command, cwd, prefix, timeout=timeout, env=env)
    capture = result.capture_report
    require(type(capture.get("exit")) is int and capture["exit"] == result.returncode and
        capture.get("timed_out") is False and capture.get("output_overflow") is False and
        capture.get("reaped") is True and capture.get("pipe_capture_complete") is True and
        capture.get("reader_errors") == {}, "complete actual process capture")
    require(capture["command"] == [str(arg) for arg in command] and
        Path(capture["command"][0]).resolve(strict=True) == executable, "actual executable and raw command")
    require(binary_before == pin(executable), "executable unchanged through process")
    require(capture == json.loads((Path(cwd) / (prefix + ".capture.json")).read_text()), "raw capture equality")
    require(result.stdout == (Path(cwd) / (prefix + ".stdout")).read_bytes() and
        result.stderr == (Path(cwd) / (prefix + ".stderr")).read_bytes(), "exact physical streams")
    result.binary_before = binary_before
    result.binary_after = pin(executable)
    return result


def resources_at(root):
    for name, data in {
        "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
        "lang/stats_US.lang": b"# Authored Lava-animation resource\n",
        "achievement/map.txt": b"",
    }.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def native_success(result):
    require(type(result.returncode) is int and result.returncode == 0 and result.stderr == b"", "native process/diagnostics")
    lines = result.stdout.splitlines()
    require(lines[:-1] == [b"16 achievements", b"151 recipes"] * 4, "four actual independent Registry banners")
    match = re.fullmatch(rb"Lava animation: ([1-9][0-9]*) checks", lines[-1]) if lines else None
    require(match is not None, "completed native checks")
    return int(match.group(1))
