"""Authored ZIP and image inputs. No game resources or captured outputs."""

from pathlib import Path
import struct
import warnings
import zipfile


def write_zip(path, records, method=zipfile.ZIP_STORED):
    with zipfile.ZipFile(path, "w") as archive:
        for name, data in records:
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = method
            info.create_system = 0
            info.external_attr = 0x10 if name.endswith("/") else 0x20
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", UserWarning)
                archive.writestr(info, data)


def zip_inputs(destination):
    """Small deterministic archives covering real contents and failure timing."""
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    records = [("a.txt", b"first"), ("pack.txt", b"one\r\ntwo\n")]
    for name, method in (("stored", zipfile.ZIP_STORED), ("deflated", zipfile.ZIP_DEFLATED)):
        write_zip(destination / (name + ".zip"), records, method)
    write_zip(destination / "duplicate.zip", [records[0], ("a.txt", b"second"), records[1]])
    write_zip(destination / "directory.zip", [("a.txt/", b""), records[1]])
    write_zip(destination / "exact-before-directory.zip", [records[0], ("a.txt/", b"")])
    stored = (destination / "stored.zip").read_bytes()
    for name in ("bad-crc", "trailing", "bad-local", "bad-central"):
        data = bytearray(stored)
        if name == "bad-crc":
            struct.pack_into("<I", data, 14, 0)
            central = data.index(b"PK\x01\x02")
            struct.pack_into("<I", data, central + 16, 0)
        elif name == "trailing":
            data.extend(b"authored-trailing-bytes")
        elif name == "bad-local":
            data[0] ^= 1
        elif name == "bad-central":
            data[data.index(b"PK\x01\x02")] ^= 1
        (destination / (name + ".zip")).write_bytes(data)
    data = bytearray((destination / "deflated.zip").read_bytes())
    local = data.index(b"PK\x03\x04")
    encoded = local + 30 + int.from_bytes(data[26:28], "little") + int.from_bytes(data[28:30], "little")
    data[encoded] = 0xff
    (destination / "bad-deflate.zip").write_bytes(data)
