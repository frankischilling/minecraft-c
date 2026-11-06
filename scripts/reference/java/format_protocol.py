"""Separate Formatter declarations from independently observed JDK outcomes."""

from __future__ import annotations

import re


EXCEPTIONS = {
    4: "java.lang.NullPointerException",
    5: "java.util.UnknownFormatConversionException",
    6: "java.util.DuplicateFormatFlagsException",
    7: "java.util.IllegalFormatFlagsException",
    8: "java.util.FormatFlagsConversionMismatchException",
    9: "java.util.MissingFormatWidthException",
    10: "java.util.IllegalFormatWidthException",
    11: "java.util.IllegalFormatPrecisionException",
    12: "java.util.IllegalFormatArgumentIndexException",
    13: "java.util.MissingFormatArgumentException",
    14: "java.util.IllegalFormatConversionException",
}
FLAG_BITS = {"-": 1, "^": 2, "#": 4, "+": 8, " ": 16,
             "0": 32, ",": 64, "(": 128, "<": 256}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, minimum=0, maximum=2147483647):
    require(re.fullmatch(r"0|[1-9][0-9]*|-[1-9][0-9]*", value) is not None,
            "noncanonical integer")
    result = int(value)
    require(minimum <= result <= maximum, "integer range")
    return result


def rows(raw):
    require(isinstance(raw, bytes) and 0 < len(raw) <= 4 * 1024 * 1024,
            "bounded observation")
    require(raw.endswith(b"\n") and b"\r" not in raw and
            all(byte in (9, 10) or 32 <= byte <= 126 for byte in raw), "ASCII LF transport")
    result = [line.split("\t") for line in raw[:-1].decode("ascii").split("\n")]
    require(len(result) <= 65536 and all(all(field for field in row) for row in result),
            "bounded nonempty fields")
    return result


def encode(source):
    return ("\n".join("\t".join(row) for row in source) + "\n").encode("ascii")


def text(source, nullable=False):
    if nullable and source == ["NULL"]:
        return "NULL"
    require(len(source) == 3 and source[0] == "TEXT", "typed UTF16 String")
    length = integer(source[1], 0, 8192)
    value = source[2]
    require(value == "-" if length == 0 else
            len(value) == length * 4 and re.fullmatch(r"[0-9a-f]+", value) is not None,
            "UTF16 length and lowercase encoding")
    return value


def decoded(value):
    require(value not in ("NULL", "-"), "nonempty String")
    return "".join(chr(int(value[i:i + 4], 16)) for i in range(0, len(value), 4))


def parse(raw):
    """Return input-only bytes and observed outcomes, without predicting formatting.

    Java exception classes map to the public native status enum. Typed public
    getters remain separate expected operands. Messages and normalized format
    specifiers are retained in the raw fixture but are outside the native API.
    The package-private argument-index getter is explicitly unavailable.
    """
    source = rows(raw)
    position = 0

    def take(tag):
        nonlocal position
        require(position < len(source) and source[position][0] == tag, "expected " + tag)
        value = source[position]
        position += 1
        return value

    require(take("FORMAT_STRINGS_V1") == ["FORMAT_STRINGS_V1"], "header")
    require(take("ENV") == ["ENV", "LOCALE", "java.util.Locale.ENGLISH"], "English locale")
    separator = take("ENV")
    require(separator[1:2] == ["LINE_SEPARATOR"], "line separator declaration")
    line_separator = text(separator[2:])
    require(line_separator in ("000a", "000d000a"), "actual supported platform separator")
    declared = [["FORMAT_INPUT_V1"], ["ENV", "LINE_SEPARATOR", line_separator]]
    observed = []
    while position < len(source) and source[position][0] == "CASE":
        index = str(len(observed))
        require(take("CASE") == ["CASE", index] and len(observed) < 1024, "case sequence")
        pattern = take("INPUT")
        require(pattern[1:3] == [index, "PATTERN"], "pattern declaration")
        pattern_value = text(pattern[3:], nullable=True)
        arguments = take("INPUT")
        require(arguments[1:3] == [index, "ARGUMENTS"], "argument-array declaration")
        if arguments[3:] == ["NULL"]:
            count, count_value = 0, "NULL"
        else:
            require(len(arguments) == 5 and arguments[3] == "COUNT", "argument count")
            count = integer(arguments[4], 0, 32)
            count_value = str(count)
        declared.append(["CASE", index, pattern_value, count_value])
        for ordinal in range(count):
            argument = take("INPUT")
            require(argument[1:3] == [index, "ARGUMENT_" + str(ordinal)], "argument declaration")
            declared.append(["ARG", text(argument[3:], nullable=True)])
        declared.append(["RUN"])
        result = take("RESULT")
        require(result[1:2] == [index] and len(result) >= 4, "result owner")
        details = {}
        if result[2] == "OK":
            outcome = {"status": "OK", "text": text(result[3:])}
        else:
            require(result[2] == "ERROR" and len(result) == 4 and
                    result[3] in EXCEPTIONS.values(), "observed Java exception class")
            outcome = {"status": "ERROR", "exception": result[3]}
            while position < len(source) and source[position][0] == "DETAIL":
                detail = take("DETAIL")
                require(len(detail) >= 4 and detail[1] == index and detail[2] not in details,
                        "unique detail owner")
                key, value = detail[2], detail[3:]
                if key in ("MESSAGE", "FLAGS", "SPECIFIER") or (key == "CONVERSION" and value[0] == "TEXT"):
                    text(value)
                elif key in ("WIDTH", "PRECISION"):
                    require(len(value) == 2 and value[0] == "INT", "integer getter")
                    integer(value[1], -2147483648)
                elif key == "CONVERSION":
                    require(len(value) == 2 and value[0] == "UNIT", "character getter")
                    integer(value[1], 0, 65535)
                elif key == "ARGUMENT_CLASS":
                    require(value == ["java.lang.String"], "reached String-only profile")
                else:
                    require(key == "ARGUMENT_INDEX" and value == ["UNAVAILABLE_NONPUBLIC"],
                            "unavailable nonpublic getter")
                details[key] = value
            require("MESSAGE" in details, "actual exception message retained")
        outcome["details"] = details
        observed.append(outcome)
    require(take("END") == ["END", str(len(observed))] and observed and position == len(source),
            "complete observation footer")
    declared.append(["END"])
    return encode(declared), observed


def compare(raw, observed):
    source = rows(raw)
    require(len(source) == len(observed) + 1 and source[-1] == ["END", str(len(observed))],
            "complete native footer")
    operands = 0
    for index, (result, expected) in enumerate(zip(source[:-1], observed)):
        label = "case " + str(index)
        require(result[:3] == ["RESULT", str(index), expected["status"]], label + " status")
        if expected["status"] == "OK":
            require(result == ["RESULT", str(index), "OK", expected["text"]], label + " UTF16 output")
            continue
        require(len(result) == 10, label + " native error shape")
        status = integer(result[3], 0, 15)
        require(EXCEPTIONS.get(status) == expected["exception"], label + " exception category")
        value = integer(result[4], -2147483648)
        flags = integer(result[5], 0, 511)
        conversion = integer(result[6], 0, 65535)
        date_time = integer(result[7], 0, 1)
        integer(result[8], 0, 8192)
        integer(result[9], 0, 8192)
        for key, detail in expected["details"].items():
            if key in ("WIDTH", "PRECISION"):
                require(value == int(detail[1]), label + " " + key)
            elif key == "FLAGS":
                actual_flags = decoded(text(detail))
                require(all(char in FLAG_BITS for char in actual_flags), label + " flag alphabet")
                require(flags == sum(FLAG_BITS[char] for char in actual_flags), label + " flags")
            elif key == "CONVERSION":
                if detail[0] == "UNIT":
                    require(conversion == int(detail[1]), label + " conversion character")
                else:
                    actual_conversion = decoded(text(detail))
                    native_conversion = ("t" if date_time else "") + chr(conversion)
                    require(native_conversion == actual_conversion, label + " unknown conversion")
            else:
                continue
            operands += 1
    return {"cases": len(observed), "typed_error_operands": operands}
