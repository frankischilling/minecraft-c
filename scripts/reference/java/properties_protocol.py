"""Validate and project the finite, actual-JDK Properties observation protocol.

This module does not parse Java properties bytes. Map contents, String identities,
and failure prefixes come exclusively from the observed rows. The input projection
uses only declared CASE/INPUT/QUERY rows and synthetic RUN/section delimiters.
"""

import re


_MAX_BYTES = 16 * 1024 * 1024
_MAX_ROWS = 65536
_MAX_INPUT = 262144
_MAX_UNITS = 65536
_MAX_STRINGS = 512
_INT_MAX = 2147483647
_SHAPES = ("ALL", "1", "7", "8192")
_POLICY = (
    "POLICY",
    "ACTUAL_JDK_PROPERTIES_LOAD_INPUT_STREAM",
    "DEFAULTS_NULL",
    "SORT_MAP_KEYS_ONLY",
    "OBSERVED_STRING_IDENTITY",
    "FINITE_DECLARED_STREAMS",
)
_UINT = re.compile(r"(?:0|[1-9][0-9]*)\Z")
_HEX = re.compile(r"[0-9a-f]+\Z")
_LABEL = re.compile(r"[a-z0-9-]{1,64}\Z")


class _Reader:
    def __init__(self, raw):
        if not isinstance(raw, bytes):
            raise ValueError("protocol: raw must be bytes")
        if not raw or len(raw) > _MAX_BYTES:
            raise ValueError("protocol: byte count outside finite bound")
        if not raw.endswith(b"\n"):
            raise ValueError("protocol: missing final LF")
        if re.fullmatch(rb"[\x09\x0a\x20-\x7e]*", raw) is None:
            raise ValueError("protocol: only printable ASCII, TAB and LF admitted")
        self.rows = raw[:-1].decode("ascii").split("\n")
        if len(self.rows) > _MAX_ROWS:
            raise ValueError("protocol: too many rows")
        if any(not row or len(row) > 2 * _MAX_INPUT + 64 for row in self.rows):
            raise ValueError("protocol: empty or oversized row")
        self.index = 0
        self.owner = None
        self.step = None

    def fail(self, reason):
        context = f"row {max(1, self.index)}"
        if self.owner is not None:
            context += f" owner {self.owner}"
        if self.step is not None:
            context += f" step {self.step}"
        raise ValueError(f"{context}: {reason}")

    def require(self, condition, reason):
        if not condition:
            self.fail(reason)

    def peek(self):
        if self.index == len(self.rows):
            return None
        return self.rows[self.index].split("\t", 1)[0]

    def take(self, tag, columns):
        if self.index == len(self.rows):
            self.fail(f"expected {tag}, reached end")
        row = self.rows[self.index].split("\t")
        self.index += 1
        self.require(len(row) == columns and row[0] == tag,
                     f"expected {tag} with {columns} columns")
        return row

    def uint(self, text, maximum=_INT_MAX):
        self.require(len(text) <= 10 and _UINT.fullmatch(text) is not None,
                     "noncanonical unsigned decimal")
        value = int(text)
        self.require(value <= maximum, "decimal outside finite bound")
        return value

    def section(self, row):
        self.require(self.uint(row[1], 239) == self.owner, "owner mismatch")
        self.require(self.uint(row[2], 2) == self.step, "step mismatch")

    def units(self, length, text, nullable=False):
        if length == "-1":
            self.require(nullable and text == "NULL", "invalid null UTF16")
            return None
        count = self.uint(length, _MAX_UNITS)
        if count == 0:
            self.require(text == "-", "empty UTF16 must use dash")
            return ()
        self.require(len(text) == 4 * count and _HEX.fullmatch(text) is not None,
                     "invalid lowercase UTF16 units")
        return tuple(int(text[index:index + 4], 16)
                     for index in range(0, len(text), 4))

    def reference(self, text, nullable=False):
        if text == "NULL":
            self.require(nullable, "unexpected null String reference")
            return None
        self.require(text.startswith("R:"), "invalid String reference")
        value = self.uint(text[2:], _MAX_STRINGS)
        self.require(value != 0, "String reference zero")
        return value


def _append(rows, row):
    rows.append("\t".join(row))


def _literal_units(text):
    return tuple(ord(character) for character in text)


def _load(reader, input_rows, expected_rows, previous_strings):
    """Read one declared input and its actual outcome, without decoding INPUT."""
    row = reader.take("INPUT", 6)
    reader.section(row)
    fail_after = -1 if row[3] == "-1" else reader.uint(row[3], _MAX_INPUT)
    byte_count = reader.uint(row[4], _MAX_INPUT)
    reader.require(fail_after <= byte_count, "failAfter exceeds input length")
    if byte_count == 0:
        reader.require(row[5] == "-", "empty byte input must use dash")
    else:
        reader.require(len(row[5]) == 2 * byte_count
                       and _HEX.fullmatch(row[5]) is not None,
                       "invalid lowercase input bytes")
    input_specification = tuple(row[2:])
    _append(input_rows, row)
    queries = []
    query_specifications = []
    while reader.peek() == "QUERY":
        row = reader.take("QUERY", 9)
        reader.section(row)
        reader.require(len(queries) < _MAX_STRINGS, "too many queries in load")
        reader.require(reader.uint(row[3], _MAX_STRINGS) == len(queries),
                       "noncontiguous query number")
        key = reader.units(row[4], row[5])
        mode = row[6]
        reader.require(mode in ("ONE", "TWO_NULL", "TWO_VALUE"),
                       "unknown query mode")
        fallback = reader.units(row[7], row[8], nullable=True)
        reader.require((fallback is not None) == (mode == "TWO_VALUE"),
                       "query fallback does not match mode")
        queries.append((key, mode, fallback))
        query_specifications.append(tuple(row[2:]))
        _append(input_rows, row)
    _append(input_rows, ("RUN", str(reader.owner), str(reader.step)))

    row = reader.take("LOAD", 14)
    reader.section(row)
    exception_class = reader.units(row[4], row[5], nullable=True)
    message = reader.units(row[6], row[7], nullable=True)
    consumed, calls, single, bulk, close, closed = (
        reader.uint(value) for value in row[8:14]
    )
    reader.require(consumed <= byte_count, "consumed bytes exceed input")
    reader.require(calls == single + bulk and calls > 0,
                   "invalid stream call accounting")
    reader.require(close == 0 and closed == 0, "load closed supplied stream")
    if row[3] == "OK":
        reader.require(exception_class is None and message is None,
                       "successful load has exception metadata")
        status = "OK"
    else:
        reader.require(row[3] == "THREW", "unknown load result")
        if exception_class == _literal_units("java.io.IOException"):
            status = "IO"
            reader.require(fail_after >= 0 and consumed == fail_after,
                           "IOException does not match finite stream boundary")
            reader.require(message == _literal_units(
                f"properties-observer read failure at byte {fail_after}"),
                "IOException message does not match declared helper stream")
        elif exception_class == _literal_units("java.lang.IllegalArgumentException"):
            status = "MALFORMED"
        else:
            reader.fail("unexpected observed exception class")
    _append(expected_rows, ("LOAD", row[1], row[2], status, *row[8:14]))

    row = reader.take("SNAPSHOT", 4)
    reader.section(row)
    entry_count = reader.uint(row[3], _MAX_STRINGS)
    _append(expected_rows, row)
    entries = []
    active_references = set()
    for _ in range(entry_count):
        row = reader.take("ENTRY", 5)
        reader.section(row)
        key_id = reader.reference(row[3])
        value_id = reader.reference(row[4])
        reader.require(key_id not in active_references,
                       "repeated active key/value String identity")
        active_references.add(key_id)
        reader.require(value_id not in active_references,
                       "repeated active key/value String identity")
        active_references.add(value_id)
        entries.append((key_id, value_id))
        _append(expected_rows, row)
    gets = []
    for index, (_, mode, _) in enumerate(queries):
        row = reader.take("GET", 6)
        reader.section(row)
        reader.require(reader.uint(row[3], _MAX_STRINGS) == index,
                       "noncontiguous GET number")
        returned = reader.reference(row[4], nullable=True)
        reader.require(row[5] == "NA" if mode == "ONE" else row[5] in ("0", "1"),
                       "GET fallback identity flag does not match mode")
        gets.append((returned, row[5]))
        _append(expected_rows, row)

    row = reader.take("STRINGS", 4)
    reader.section(row)
    string_count = reader.uint(row[3], _MAX_STRINGS)
    old_count = len(previous_strings)
    reader.require(string_count >= old_count, "held String prefix shrank")
    _append(expected_rows, row)
    strings = []
    for index in range(string_count):
        row = reader.take("STRING", 6)
        reader.section(row)
        reader.require(reader.uint(row[3], _MAX_STRINGS) == index + 1,
                       "noncontiguous String identity")
        strings.append(reader.units(row[4], row[5]))
        if index < old_count:
            reader.require(strings[-1] == previous_strings[index],
                           "held String identity changed content")
        _append(expected_rows, row)
    row = reader.take("END_LOAD", 3)
    reader.section(row)
    _append(expected_rows, row)

    def content(identity):
        reader.require(identity is not None and identity <= string_count,
                       "String reference has no physical STRING row")
        return strings[identity - 1]

    observed_map = {}
    previous_key = None
    for key_id, value_id in entries:
        key = content(key_id)
        content(value_id)
        reader.require(previous_key is None or previous_key < key,
                       "ENTRY keys are not unique unsigned-UTF16 sorted units")
        previous_key = key
        observed_map[key] = value_id
    fallback_references = set()
    for (key, mode, fallback), (returned, same) in zip(queries, gets):
        if returned is not None:
            content(returned)
        if key in observed_map:
            reader.require(returned == observed_map[key]
                           and same == ("NA" if mode == "ONE" else "0"),
                           "GET differs from observed present entry")
        elif mode == "ONE":
            reader.require(returned is None and same == "NA",
                           "missing one-argument GET is not null")
        elif mode == "TWO_NULL":
            reader.require(returned is None and same == "1",
                           "missing null-fallback GET identity mismatch")
        else:
            reader.require(returned is not None and content(returned) == fallback
                           and same == "1", "missing value-fallback GET mismatch")
            reader.require(returned > old_count and returned not in active_references
                           and returned not in fallback_references,
                           "fresh query fallback reused another String identity")
            fallback_references.add(returned)

    # The helper enrolls sorted ENTRY key/value references, then GET results.
    # Validate that observed order, without inventing identities from text equality.
    seen = set(range(1, old_count + 1))
    next_identity = old_count + 1
    references = [identity for entry in entries for identity in entry]
    references.extend(returned for returned, _ in gets if returned is not None)
    for identity in references:
        if identity not in seen:
            reader.require(identity == next_identity,
                           "first-observed String identity order mismatch")
            seen.add(identity)
            next_identity += 1
    reader.require(next_identity == string_count + 1,
                   "unreferenced newly published STRING row")
    specification = (input_specification, tuple(query_specifications))
    return strings, len(queries), specification


def parse(raw: bytes) -> dict:
    """Return input/expected ASCII TSV bytes and validated finite-corpus counts.

    Raises ValueError with row/owner/step context on malformed observations. The
    input footer keeps the original rows-before-footer count; the native expected
    footer counts its own projected rows. No Properties entries are synthesized.
    """
    reader = _Reader(raw)
    reader.take("PROPERTIES_OBSERVATION_V1", 1)
    reader.require(tuple(reader.take("POLICY", 6)) == _POLICY,
                   "unexpected observation policy")
    input_rows = ["PROPERTIES_INPUT_V1"]
    expected_rows = ["PROPERTIES_NATIVE_V1"]
    labels = {}
    load_count = 0
    query_count = 0
    for owner in range(240):
        reader.owner = owner
        reader.step = None
        row = reader.take("CASE", 5)
        reader.require(reader.uint(row[1], 239) == owner,
                       "owners must be contiguous 0 through 239")
        label, shape = row[2], row[3]
        reader.require(_LABEL.fullmatch(label) is not None, "invalid case label")
        reader.require(shape == _SHAPES[owner % 4], "unexpected stream-shape order")
        step_count = reader.uint(row[4], 3)
        reader.require(step_count >= 1, "case has no load steps")
        if owner % 4 == 0:
            reader.require(label not in labels, "duplicate case label")
            labels[label] = (step_count, [])
        else:
            reader.require(label == previous_label, "shape group changes case label")
            reader.require(step_count == labels[label][0],
                           "shape group changes declared load count")
        previous_label = label
        _append(input_rows, row)
        _append(expected_rows, row)
        strings = []
        for step in range(step_count):
            reader.step = step
            strings, queries, specification = _load(
                reader, input_rows, expected_rows, strings
            )
            if owner % 4 == 0:
                labels[label][1].append(specification)
            else:
                reader.require(specification == labels[label][1][step],
                               "shape group changes declared input/query data")
            load_count += 1
            query_count += queries
        reader.step = None
        row = reader.take("END_CASE", 2)
        reader.require(reader.uint(row[1], 239) == owner, "END_CASE owner mismatch")
        _append(input_rows, row)
        _append(expected_rows, row)

    reader.owner = None
    footer = reader.take("END", 5)
    reader.require(tuple(reader.uint(value) for value in footer[1:4])
                   == (240, load_count, query_count), "footer count mismatch")
    reader.require(load_count == 252 and query_count == 980,
                   "unexpected finite corpus load/query totals")
    reader.require(reader.uint(footer[4], _MAX_ROWS) == len(reader.rows) - 1,
                   "raw rows-before-footer mismatch")
    reader.require(reader.index == len(reader.rows), "rows after END")
    _append(input_rows, footer)
    _append(expected_rows, ("END", "240", str(load_count), str(query_count),
                            str(len(expected_rows))))
    projected_input = ("\n".join(input_rows) + "\n").encode("ascii")
    projected_expected = ("\n".join(expected_rows) + "\n").encode("ascii")
    reader.require(len(projected_input) <= _MAX_BYTES
                   and len(projected_expected) <= _MAX_BYTES,
                   "projected byte count exceeds finite bound")
    return {
        "input": projected_input,
        "expected": projected_expected,
        "counts": {
            "owners": 240,
            "loads": load_count,
            "queries": query_count,
            "raw_rows": len(reader.rows),
            "raw_rows_before_footer": len(reader.rows) - 1,
            "input_rows": len(input_rows),
            "expected_rows": len(expected_rows),
        },
    }
