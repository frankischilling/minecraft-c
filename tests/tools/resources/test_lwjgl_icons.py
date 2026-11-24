"""Generated structural import fixtures; no SDK assets, Java or native process."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("sdk_icon_import", ROOT / "scripts/resources/lwjgl_icons.py")
if SPEC is None or SPEC.loader is None:
    raise ValueError("actual SDK icon import source")
icons = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(icons)


def u2(value):
    return struct.pack(">H", value)


def u4(value):
    return struct.pack(">I", value)


def mutf(units):
    out = bytearray()
    for value in units:
        if 0 < value < 128:
            out.append(value)
        elif value < 2048:
            out.extend((0xc0 | (value >> 6), 0x80 | (value & 63)))
        else:
            out.extend((0xe0 | (value >> 12), 0x80 | ((value >> 6) & 63), 0x80 | (value & 63)))
    return bytes(out)


def fixture():
    # These bytes are a structural parser fixture, never a loaded VM class.
    # Original class/Code constants remain immutable outside explicit mocks.
    pool = []

    def add(value):
        pool.append(value)
        return len(pool)

    def text(value):
        raw = mutf(tuple(map(ord, value))) if isinstance(value, str) else mutf(value)
        return add(b"\x01" + u2(len(raw)) + raw)

    def class_(value):
        return add(b"\x07" + u2(text(value)))

    owner = class_("org/lwjgl/LWJGLUtil")
    parent = class_("java/lang/Object")
    code_name = text("Code")
    clinit_name, clinit_descriptor = text("<clinit>"), text("()V")
    load_name = text("loadIcon")
    load_descriptor = text("(Ljava/lang/String;)Ljava/nio/ByteBuffer;")
    load_nt = add(b"\x0c" + u2(load_name) + u2(load_descriptor))
    load_ref = add(b"\x0a" + u2(owner) + u2(load_nt))
    buffer_descriptor = text("Ljava/nio/ByteBuffer;")
    field_names, field_refs, strings = [], [], []
    source_units = ((0, 0x101, 127, 65535) * 256, (17, 53, 101, 255) * 1024)
    for edge, units in zip((16, 32), source_units):
        field = text(f"LWJGLIcon{edge}x{edge}")
        field_names.append(field)
        nt = add(b"\x0c" + u2(field) + u2(buffer_descriptor))
        field_refs.append(add(b"\x09" + u2(owner) + u2(nt)))
        strings.append(add(b"\x08" + u2(text(units))))
    code = b"".join(b"\x12" + bytes((literal,)) + b"\xb8" + u2(load_ref) + b"\xb3" + u2(field)
                     for literal, field in zip(strings, field_refs)) + b"\xb1"
    bodies = {"<clinit>": code, "loadIcon": b"\x01\xb0"}
    raw_attributes = {name: u2(4) + u2(1) + u4(len(body)) + body + u2(0) + u2(0)
                      for name, body in bodies.items()}

    def method(access, name, descriptor, key):
        raw = raw_attributes[key]
        return u2(access) + u2(name) + u2(descriptor) + u2(1) + u2(code_name) + u4(len(raw)) + raw

    raw = b"\xca\xfe\xba\xbe" + u2(0) + u2(49) + u2(len(pool) + 1) + b"".join(pool)
    raw += u2(33) + u2(owner) + u2(parent) + u2(0)
    raw += u2(2) + b"".join(u2(25) + u2(name) + u2(buffer_descriptor) + u2(0) for name in field_names)
    raw += u2(2) + method(8, clinit_name, clinit_descriptor, "<clinit>")
    raw += method(10, load_name, load_descriptor, "loadIcon") + u2(0)
    pins = {"CLASS_SHA256": hashlib.sha256(raw).hexdigest(),
            "CODE_SHA256": {name: hashlib.sha256(body).hexdigest() for name, body in bodies.items()},
            "ATTRIBUTE_SHA256": {name: hashlib.sha256(body).hexdigest() for name, body in raw_attributes.items()}}
    return raw, pins


class IconImportTests(unittest.TestCase):
    def test_modified_utf8_code_units_and_low_byte_conversion(self):
        units = (0, 1, 127, 128, 2047, 2048, 0xd800, 0xdc00, 65535)
        self.assertEqual(icons.modified_utf8_units(mutf(units)), units)
        for bad in (b"\x00", b"\x80", b"\xc1\x81", b"\xe0\x80\x81", b"\xf0\x90\x80\x80", b"\xc2"):
            with self.subTest(bad=bad), self.assertRaises(icons.IconImportFailure):
                icons.modified_utf8_units(bad)

    def test_exact_typed_clinit_operands(self):
        raw, pins = fixture()
        with mock.patch.multiple(icons, **pins):
            assets, proof = icons.extract_class(raw)
        self.assertEqual(set(assets), {"sdk/lwjgl-icon-16.rgba", "sdk/lwjgl-icon-32.rgba"})
        self.assertEqual(assets["sdk/lwjgl-icon-16.rgba"][:4], bytes((0, 1, 127, 255)))
        self.assertEqual(len(assets["sdk/lwjgl-icon-16.rgba"]), 1024)
        self.assertEqual(len(assets["sdk/lwjgl-icon-32.rgba"]), 4096)
        self.assertEqual([row["literal_pc"] for row in proof["assets"]], [0, 8])
        self.assertEqual([row["load_icon_pc"] for row in proof["assets"]], [2, 10])
        self.assertEqual([row["putstatic_pc"] for row in proof["assets"]], [5, 13])

    def test_real_bounded_zip_intake_and_identity_rejection(self):
        raw, pins = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr(icons.CLASS_PATH, raw)
            pins["SDK_SHA256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            with mock.patch.multiple(icons, **pins):
                assets, _ = icons.read_sdk(path)
                self.assertEqual(len(assets), 2)
                path.write_bytes(path.read_bytes() + b"changed")
                with self.assertRaises(icons.IconImportFailure):
                    icons.read_sdk(path)

    def test_original_pins_never_accept_generated_class(self):
        raw, pins = fixture()
        with self.assertRaises(icons.IconImportFailure):
            icons.extract_class(raw)
        with mock.patch.object(icons, "CLASS_SHA256", pins["CLASS_SHA256"]):
            with self.assertRaises(icons.IconImportFailure):
                icons.extract_class(raw)


if __name__ == "__main__":
    unittest.main()
