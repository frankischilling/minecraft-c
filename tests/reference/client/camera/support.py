"""Camera trace validation and the existing reviewed bounded capture owner."""
import importlib.util
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def run(command, cwd, prefix, timeout=180, env=None):
    path = Path(__file__).resolve().parents[1] / "statistics/support.py"
    spec = importlib.util.spec_from_file_location("camera_capture", path)
    require(spec is not None and spec.loader is not None, "reviewed capture owner")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    result = module.run_bounded(command, cwd, prefix, timeout=timeout, env=env)
    require(result.capture_report.get("reader_errors") == {}, "explicit reader success")
    return result


def resources_at(root):
    for name, data in {
        "lang/en_US.lang": b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                           b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n",
        "lang/stats_US.lang": b"# Authored camera resource\n",
        "achievement/map.txt": b"",
    }.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def native_rows(result):
    require(result.returncode == 0 and result.stderr == b"", "native exit/diagnostics")
    data = result.stdout
    require(0 < len(data) < 200000 and all(32 <= b <= 126 or b in (10, 13) for b in data), "trace ASCII extent")
    require(b"\r" not in data.replace(b"\r\n", b"\n"), "trace physical terminators")
    rows = data.splitlines()
    require(rows[:2] == [b"151 recipes", b"16 achievements"], "native actual banners")
    count = re.fullmatch(rb"First-person camera: ([1-9][0-9]*) checks", rows[-1])
    require(count is not None, "native complete positive checks")
    cases, matrices, origins, cursor = {}, {}, {}, 2
    for which in range(29):
        require(cursor < len(rows)-1, "missing camera case")
        match = re.fullmatch(rb"CAMERA ([0-9]+) ([0-9]+) ([adeg-]) ([0-9]+) ([0-9]+)(?: ([0-9a-f]{8})){4}", rows[cursor])
        require(match is not None and int(match[1]) == which, "ordered camera case")
        cells = rows[cursor].split()
        status, operations = int(cells[2]), int(cells[5])
        expected = 8 if which in (3, 11) else 3 if which in (17, 18, 19, 28) else 4 if which == 20 else 6 if which == 21 else 0
        require(status == expected and operations <= 32, "camera source/native failure inventory")
        site = (b"a", b"292") if which == 3 else (b"g", b"292") if which == 11 else (
            (b"g", b"159") if which == 28 else (b"a", b"11") if which in (17, 19) else (b"d", b"15") if which in (18, 21)
            else (b"a", b"15") if which == 20 else (b"-", b"0"))
        require(tuple(cells[3:5]) == site, "exact reached diagnostic site")
        shape = []
        if which not in (17, 18, 19, 20, 21):
            if which in (5, 6):
                shape.append((0, 1, "a", 55))
            shape.append((2, 1, "a", 160))
            if which in (5, 6):
                shape.append((0, 0, "a", 196))
            if which == 7:
                shape.append((1, 0, "e", 46))
            if which in (2, 14):
                shape.extend((1, 0, "e", pc) for pc in (90, 101, 109))
            if which >= 1:
                shape.extend(((0, 0, "f", 112), (1, 0, "f", 132),
                              (1, 0, "f", 158), (1, 0, "f", 166)))
            if which != 3:
                shape.append((1, 0, "g", 95))
                if which != 11:
                    if which in (9, 10, 26, 27, 28):
                        shape.append((0, 0, "g", 115))
                        if which not in (10, 28):
                            if which in (26, 27):
                                shape.append((1, 0, "g", 223))
                            shape.extend(((1, 0, "g", 249), (1, 0, "g", 272)))
                    else:
                        shape.append((0, 0, "g", 721))
                    if which not in (8, 10, 28):
                        shape.extend(((1, 0, "g", 756), (1, 0, "g", 781)))
                    if which != 28:
                        shape.append((0, 0, "g", 787))
        require(operations == len(shape), "source operation count")
        cases[which] = cells
        cursor += 1
        if status == 0:
            require(re.fullmatch(rb"ORIGIN " + str(which).encode() + rb"(?: [0-9a-f]{16}){4}", rows[cursor]) is not None, "origin row")
            origins[which] = rows[cursor]
            cursor += 1
            for label in (b"PROJECTION", b"MODELVIEW"):
                require(re.fullmatch(label + b" " + str(which).encode() + rb"(?: [0-9a-f]{8}){16}", rows[cursor]) is not None, "matrix row")
                matrices[label.decode(), which] = rows[cursor].split()[2:]
                cursor += 1
        for expected_op in shape:
            require(cursor < len(rows)-1 and re.fullmatch(rb"OP " + str(which).encode() + rb" [012] [01] [aefg] [0-9]+(?: [0-9a-f]{8}){4}", rows[cursor]) is not None, "operation row")
            require(tuple(rows[cursor].split()[2:6]) == tuple(str(x).encode() for x in expected_op), "source operation order")
            cursor += 1
    require(cursor == len(rows)-1, "no trailing camera trace")
    return {"checks": int(count[1]), "cases": cases, "matrices": matrices, "origins": origins}


def callback_rows(result):
    require(result.returncode == 0 and result.stderr == b"", "callback exit/diagnostics")
    rows = result.stdout.splitlines()
    require(0 < len(result.stdout) < 30000, "callback trace extent")
    cursor, matrices, results = 0, {}, {}
    inventory = [(0, 0, 9), (0, 0, 9), (0, 0, 11), (3, 203, 5), (3, 731, 8),
                 (4, 185, 5), (4, 62, 1), (4, 15, 1), (0, 0, 8), (6, 15, 1),
                 (4, 0, 0), (4, 219, 5), (4, 238, 5), (4, 35, 1), (2, 179, 5), (6, 15, 1)]
    for which, expected in enumerate(inventory):
        require(rows[cursor:cursor + 2] == [b"151 recipes", b"16 achievements"], "callback actual banners")
        cursor += 2
        if which <= 4:
            for label in (b"CBPROJECTION", b"CBMODELVIEW"):
                require(cursor < len(rows) and re.fullmatch(label + b" " + str(which).encode() +
                        rb"(?: [0-9a-f]{8}){16}", rows[cursor]) is not None, "callback matrix row")
                matrices[label.decode(), which] = rows[cursor].split()[2:]
                cursor += 1
        expected_row = ("CALLBACK %d %d %d %d" % (which, *expected)).encode()
        require(cursor < len(rows) and rows[cursor] == expected_row, "ordered exact callback outcome")
        results[which] = expected
        cursor += 1
    require(cursor + 1 == len(rows), "callback complete inventory")
    count = re.fullmatch(rb"Camera callbacks: ([1-9][0-9]*) checks", rows[cursor])
    require(count is not None, "callback positive checks")
    return {"checks": int(count[1]), "cases": results, "matrices": matrices}
