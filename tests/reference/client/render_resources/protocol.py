"""Strict stream and process admission for independent resource observations."""
import hashlib
import os
from pathlib import Path
import re

CPU_NAMES = (
    "geLists", "geC0", "geC17", "geCNegative", "geD0", "geD1024",
    "geDWrappedZero", "geDNegative", "geE3", "geENegative", "views",
    "nwInitial", "nwBegin", "nwBeginAgain", "nwVertex0", "nwVertex1",
    "nwVertex2", "nwVertex3", "nwResetBody", "geListsAfterCpu",
)
FONT_NAMES = (
    "not-image56", "png-widths242", "ready-288-commands", "constructor-reentry",
    "execute-all-288", "measure-draw-capture", "draw-reentry",
    "actual-App-d-e-J-lifetime", "actual-mk165-frontier",
    "failed580-retire-busy-retry", "native-retirement",
)


def require(value, message):
    if not value:
        raise ValueError(message)


def fingerprint(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def records(data):
    require(type(data) is bytes and 0 < len(data) <= 2 * 1024 * 1024, "stream extent/type")
    require(data.endswith(b"\n"), "complete final line")
    normalized = data.replace(b"\r\n", b"\n")
    require(all(c == 10 or 32 <= c <= 126 for c in normalized), "ASCII LF/CRLF delimiters only")
    rows = normalized[:-1].split(b"\n")
    require(all(rows), "no empty records")
    return rows


def _cursor(value, order=False):
    pattern = rb"(0|[1-9][0-9]*),(0|[1-9][0-9]*),(0|[1-9][0-9]*),true"
    if order:
        pattern += rb",(?:LITTLE_ENDIAN|BIG_ENDIAN)"
    found = re.fullmatch(pattern, value)
    require(found is not None, "typed direct-buffer cursor")
    position, limit, capacity = map(int, found.groups()[:3])
    require(position <= limit <= capacity <= 8388608, "cursor bounds")


def _state(value):
    names = (b"h", b"o", b"p", b"r", b"k", b"l", b"m", b"n", b"q", b"w", b"x",
             b"z", b"A", b"B", b"d", b"e", b"f", b"gLength", b"g")
    fields = value.split(b";")
    require(len(fields) == len(names), "nw exact field inventory")
    parsed = {}
    for name, field in zip(names, fields):
        require(field.startswith(name + b"="), "nw ordered field identity")
        parsed[name] = field[len(name) + 1:]
    for name in (b"l", b"m", b"n", b"q", b"w", b"x"):
        require(parsed[name] in (b"true", b"false"), "nw Boolean")
    for name in (b"h", b"o", b"p", b"r", b"k", b"z", b"A", b"B", b"gLength"):
        require(re.fullmatch(rb"-?(?:0|[1-9][0-9]*)", parsed[name]) is not None, "nw integer")
        require(-(2**31) <= int(parsed[name]) < 2**31, "nw int32")
    for name in (b"d", b"e", b"f"):
        _cursor(parsed[name])
    count = int(parsed[b"o"])
    require(0 <= count <= int(parsed[b"gLength"]) <= 2097152, "nw live array span")
    words = parsed[b"g"].split(b",") if parsed[b"g"] else []
    require(len(words) == count and all(re.fullmatch(rb"[0-9a-f]{8}", w) for w in words),
            "actual ordered nw words")


def cpu_rows(data):
    rows = records(data)
    require(len(rows) == len(CPU_NAMES), "exact20 CPU record inventory")
    for name, row in zip(CPU_NAMES, rows):
        label, sep, payload = row.partition(b"|")
        require(sep and label == name.encode("ascii"), "ordered CPU identity")
        if name == "geLists":
            require(payload == b"true|0,0", "actual distinct empty Lists")
        elif name == "geListsAfterCpu":
            require(payload == b"true,true|0,0", "unchanged List identities and counts")
        elif name.endswith("Negative"):
            require(payload == b"java.lang.IllegalArgumentException", "actual negative source outcome")
        elif name.startswith("ge"):
            require(payload.startswith(b"OK|"), "buffer completion")
            _cursor(payload[3:], True)
        elif name == "views":
            cells = payload.split(b"|")
            require(len(cells) == 4 and re.fullmatch(rb"[0-9a-f]{8},[0-9a-f]{8}", cells[0]),
                    "actual alias words")
            for cell in cells[1:]:
                _cursor(cell)
        else:
            if name == "nwInitial":
                require(payload.startswith(b"b=true;c=false;"), "actual CPU class flags")
                payload = payload[len(b"b=true;c=false;"):]
            elif name == "nwBeginAgain":
                prefix = b"java.lang.IllegalStateException|Already tesselating!|"
                require(payload.startswith(prefix), "actual repeated begin outcome")
                payload = payload[len(prefix):]
            _state(payload)
    return rows


def font_rows(data):
    rows = records(data)
    require(len(rows) == len(FONT_NAMES) + 2, "complete font control inventory")
    device = rows[0].split(b"|")
    require(len(device) == 5 and device[0] == b"DEVICE", "actual GL device record")
    version = re.fullmatch(rb"([0-9]+),([0-9]+),1", device[1])
    require(version is not None and (int(version[1]), int(version[2])) >= (3, 3), "actual core GL>=3.3")
    require(all(re.fullmatch(rb"(?:[0-9a-f]{2}){1,160}", cell) for cell in device[2:]),
            "actual bounded provider strings")
    require(rows[1:-1] == [("FONT " + name + " PASS").encode("ascii") for name in FONT_NAMES],
            "ordered genuine font controls")
    footer = re.fullmatch(rb"source font: ([1-9][0-9]*) checks passed", rows[-1])
    require(footer is not None and int(footer[1]) >= 600, "positive completed native contract")
    return {"rows": len(FONT_NAMES), "checks": int(footer[1]),
            "device": {"version": [int(version[1]), int(version[2])], "profile": "CORE",
                       "vendor_hex": device[2].decode(), "renderer_hex": device[3].decode(),
                       "version_hex": device[4].decode()}}


def completed(capture, stdout, stderr, executable, *, empty_stderr=True):
    require(type(capture) is dict, "capture record")
    require(type(capture.get("exit")) is int and capture["exit"] == 0, "successful real child exit")
    for key in ("reaped", "pipe_capture_complete"):
        require(capture.get(key) is True, key)
    for key in ("timed_out", "output_overflow"):
        require(capture.get(key) is False, key)
    require(capture.get("reader_errors") == {}, "explicit successful readers")
    require(type(capture.get("stdout_bytes")) is int and capture["stdout_bytes"] == len(stdout) and
            type(capture.get("stderr_bytes")) is int and capture["stderr_bytes"] == len(stderr),
            "actual retained stream lengths")
    require(not empty_stderr or stderr == b"", "empty child stderr")
    command = capture.get("command")
    require(type(command) is list and command and all(type(x) is str for x in command), "actual command")
    require(os.path.samefile(command[0], executable), "actual direct executable identity")
    return capture
