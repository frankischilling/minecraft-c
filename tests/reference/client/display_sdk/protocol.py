"""Strict completion marker for the authored native SDK-owner controls."""
import hashlib
import re

MARKERS = {"queue": b"SDK_QUEUE_OK", "icons": b"SDK_ICONS_OK",
           "transport": b"SDK_TRANSPORT_OK", "window": b"SDK_WINDOW_OK"}


def require(value, message):
    if not value:
        raise ValueError(message)


def footer(data, kind):
    require(kind in MARKERS and type(data) is bytes and 0 < len(data) <= 4096, "native kind/stream bound")
    require(data.endswith(b"\n"), "complete native footer")
    normalized = data.replace(b"\r\n", b"\n")
    require(all(byte == 10 or 32 <= byte <= 126 for byte in normalized), "ASCII LF/CRLF delimiters only")
    found = re.fullmatch(re.escape(MARKERS[kind]) + rb"\|checks=([1-9][0-9]*)\n", normalized)
    require(found is not None, "one exact positive native footer")
    count = int(found.group(1))
    require(count <= 1000000, "finite native check count")
    return {"kind": kind, "checks": count, "stdout_sha256": hashlib.sha256(data).hexdigest()}
