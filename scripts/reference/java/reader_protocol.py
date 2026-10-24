"""Project declared Reader inputs separately from actual JDK observations."""

from __future__ import annotations

import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, minimum=0, maximum=2147483647):
    require(re.fullmatch(r"0|[1-9][0-9]*|-[1-9][0-9]*", value) is not None,
            "noncanonical integer")
    result = int(value)
    require(minimum <= result <= maximum, "integer range")
    return result


def units(length, value):
    size = integer(length, -1, 32768)
    if size == -1:
        require(value == "NULL", "null String")
        return None
    require(value == "-" if size == 0 else
            len(value) == size * 4 and re.fullmatch(r"[0-9a-f]+", value) is not None,
            "UTF16 String transport")
    return tuple(int(value[i:i + 4], 16) for i in range(0, size * 4, 4))


def rows(raw):
    require(isinstance(raw, bytes) and 0 < len(raw) <= 4 * 1024 * 1024,
            "observation size")
    require(raw.endswith(b"\n") and b"\r" not in raw, "LF observation")
    require(all(byte in (9, 10) or 32 <= byte <= 126 for byte in raw), "ASCII observation")
    result = [line.split("\t") for line in raw[:-1].decode("ascii").split("\n")]
    require(len(result) <= 65536 and all(all(field for field in row) for row in result),
            "observation row bound or empty field")
    return result


def encode(value):
    return ("\n".join("\t".join(row) for row in value) + "\n").encode("ascii")


def parse(raw):
    """Return input-only replay bytes, independent observed expected bytes, counts.

    The only outcome mapping is actual java.io.IOException to native IO. Its
    raw class/message remains in the retained observation. No line decoding,
    predicted stream behavior or expected line content constructs replay input.
    """
    source = rows(raw)
    position = 0

    def take(tag, width):
        nonlocal position
        require(position < len(source), "truncated observation")
        value = source[position]
        position += 1
        require(len(value) == width and value[0] == tag, "expected " + tag)
        return value

    require(take("READER_OBSERVATION_V1", 1) == ["READER_OBSERVATION_V1"], "header")
    require(take("POLICY", 6) == ["POLICY", "ACTUAL_JDK_BUFFERED_READER_INPUT_STREAM_READER",
            "EXPLICIT_CHARSETS", "REPLACE_MALFORMED", "FINITE_DECLARED_STREAMS",
            "HELD_LINE_IDENTITIES"], "policy")
    declared = [["READER_INPUT_V1"]]
    expected = [["READER_NATIVE_V1"]]
    owners = actions = read_controls = close_controls = 0
    while position < len(source) and source[position][0] == "CASE":
        case = take("CASE", 7)
        owner = str(owners)
        require(case[1] == owner and owners < 1024, "owner sequence")
        require(re.fullmatch(r"[a-z0-9-]{1,64}", case[2]) is not None, "label")
        require(case[3] in ("UTF-8", "windows-1252", "ISO-8859-1", "US-ASCII"), "charset")
        require(case[4] in ("ALL", "1", "7", "8192"), "read shape")
        require(case[5] in ("ZERO", "REMAINING", "ERROR"), "availability")
        control_count = integer(case[6], 1, 16)
        data = take("INPUT", 6)
        require(data[1] == owner, "input owner")
        byte_count = integer(data[4], 0, 32768)
        integer(data[2], -1, byte_count)
        require(data[3] in ("0", "1"), "close fault")
        require(data[5] == "-" if byte_count == 0 else len(data[5]) == byte_count * 2
                and re.fullmatch(r"[0-9a-f]+", data[5]) is not None, "input byte transport")
        controls = []
        for index in range(control_count):
            query = take("QUERY", 4)
            require(query[1:3] == [owner, str(index)]
                    and query[3] in ("READ_LINE", "CLOSE"), "declared control")
            controls.append(query)
            read_controls += query[3] == "READ_LINE"
            close_controls += query[3] == "CLOSE"
        # Only these actual declarations go to the native process.
        declared.extend([case, data, *controls])
        expected.extend([case, data, *controls])
        created = take("CREATE", 9)
        require(created[1] == owner and created[2:] == ["0"] * 7, "creation has no stream effects")
        expected.append(created)
        references = set()
        prior = [0] * 7
        for index, query in enumerate(controls):
            result = take("RESULT", 17)
            require(result[1:4] == [owner, str(index), query[3]], "result control")
            status, reference = result[4:6]
            require(status in ("OK", "EOF", "THREW"), "actual result status")
            kind = units(result[6], result[7])
            message = units(result[8], result[9])
            if status == "THREW":
                require(kind == tuple(map(ord, "java.io.IOException")) and message is not None
                        and reference == "NULL", "only actual IOException maps to IO")
                status = "IO"
            else:
                require(kind is None and message is None, "successful result has no Throwable")
            if reference != "NULL":
                require(status == "OK" and query[3] == "READ_LINE"
                        and reference.startswith("R:"), "line reference")
                identity = integer(reference[2:], 1, 16)
                if identity not in references:
                    require(identity == len(references) + 1, "first-observed String identity")
                    references.add(identity)
            else:
                require(query[3] == "CLOSE" or status in ("EOF", "IO"), "nonnull successful line")
            require(status != "EOF" or query[3] == "READ_LINE", "EOF operation")
            counters = [integer(field) for field in result[10:]]
            require(counters[0] <= byte_count and counters[1] == counters[2] + counters[3]
                    and counters[2] == 0 and counters[6] <= 1
                    and all(now >= old for now, old in zip(counters, prior)), "stream counters")
            prior = counters
            expected.append([*result[:4], status, reference, *result[10:]])
        string_count = take("STRINGS", 3)
        require(string_count[1] == owner and integer(string_count[2], 0, 16) == len(references),
                "complete retained identity table")
        expected.append(string_count)
        for identity in range(1, len(references) + 1):
            string = take("STRING", 5)
            require(string[1:3] == [owner, str(identity)], "String identity sequence")
            require(units(string[3], string[4]) is not None, "nonnull held line")
            expected.append(string)
        end_case = take("END_CASE", 2)
        require(end_case[1] == owner, "case end owner")
        declared.append(end_case)
        expected.append(end_case)
        owners += 1
        actions += control_count
    end = take("END", 6)
    totals = list(map(str, (owners, actions, read_controls, close_controls)))
    require(owners > 0 and end[1:5] == totals and end[5] == str(position - 1)
            and position == len(source), "exact final observed footer")
    declared.append(["END", *totals, str(len(declared))])
    expected.append(["END", *totals, str(len(expected))])
    counts = {
        "owners": owners, "actions": actions, "read_controls": read_controls,
        "close_controls": close_controls, "observed_rows": len(source),
        "projected_rows": len(expected),
    }
    return {"input": encode(declared), "expected": encode(expected), "counts": counts}
