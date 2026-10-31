"""Pure data controls; these fixtures are not native or original observations."""
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from compare import activation
from protocol import CPU_NAMES, FONT_NAMES, completed, cpu_rows, font_rows


def cpu_fixture():
    state = b"h=0;o=0;p=0;r=7;k=0;l=false;m=false;n=false;q=false;w=true;x=false;z=0;A=10;B=2097152;"
    state += b"d=0,0,0,true;e=0,0,0,true;f=0,0,0,true;gLength=2097152;g="
    rows = []
    for name in CPU_NAMES:
        if name == "geLists": value = b"true|0,0"
        elif name == "geListsAfterCpu": value = b"true,true|0,0"
        elif name.endswith("Negative"): value = b"java.lang.IllegalArgumentException"
        elif name.startswith("ge"): value = b"OK|0,0,0,true,LITTLE_ENDIAN"
        elif name == "views": value = b"3f800001,c0100000|0,32,32,true|0,8,8,true|0,8,8,true"
        elif name == "nwInitial": value = b"b=true;c=false;" + state
        elif name == "nwBeginAgain": value = b"java.lang.IllegalStateException|Already tesselating!|" + state
        else: value = state
        rows.append(name.encode() + b"|" + value)
    return b"\n".join(rows) + b"\n"


def font_fixture():
    return b"\n".join([b"DEVICE|4,5,1|66697874757265|66697874757265|66697874757265"] +
                       [("FONT " + n + " PASS").encode() for n in FONT_NAMES] +
                       [b"source font: 700 checks passed"]) + b"\n"


class ProtocolControls(unittest.TestCase):
    def test_positive_delimiters(self):
        for parse, fixture in ((cpu_rows, cpu_fixture()), (font_rows, font_fixture())):
            for data in (fixture, fixture.replace(b"\n", b"\r\n")):
                parse(data)

    def test_rejected_separators(self):
        for parse, fixture in ((cpu_rows, cpu_fixture()), (font_rows, font_fixture())):
            for separator in (b"\x0b", b"\x0c", b"\x1c", b"\x1d", b"\x1e", b"\r", b"\x00"):
                with self.subTest(parser=parse.__name__, separator=separator), self.assertRaises(ValueError):
                    parse(fixture.replace(b"\n", separator, 1))

    def test_incomplete_reordered_or_extra(self):
        for parse, fixture in ((cpu_rows, cpu_fixture()), (font_rows, font_fixture())):
            lines = fixture.split(b"\n")[:-1]
            bad = (fixture[:-1], fixture + b"\n", fixture + b"unexpected\n",
                   b"\n".join(lines[:-1]) + b"\n",
                   b"\n".join([lines[1], lines[0], *lines[2:]]) + b"\n",
                   b"\n".join([lines[0], *lines[:-1]]) + b"\n")
            for data in bad:
                with self.subTest(parser=parse.__name__, data=data[:32]), self.assertRaises(ValueError):
                    parse(data)

    def test_schema_and_completion(self):
        cpu = cpu_fixture()
        for data in (cpu.replace(b"LITTLE_ENDIAN", b"OTHER_ENDIAN", 1),
                     cpu.replace(b"d=0,0,0,true", b"d=1,0,0,true", 1),
                     cpu.replace(b"g=\n", b"g=00000000\n", 1),
                     cpu.replace(b"h=0;o=0", b"o=0;h=0", 1),
                     cpu.replace(b"Already tesselating!", b"Already tessellating!", 1),
                     cpu.replace(b"java.lang.IllegalArgumentException", b"native-limit", 1)):
            with self.subTest(data=data[:60]), self.assertRaises(ValueError): cpu_rows(data)
        for data in (font_fixture().replace(b"700", b"0"), font_fixture().replace(b"700", b"1"),
                     font_fixture().replace(b"DEVICE|4,5,1", b"DEVICE|3,2,1"),
                     font_fixture().replace(b"FONT ready-288-commands PASS", b"FONT ready-288-commands SKIP")):
            with self.subTest(data=data[-70:]), self.assertRaises(ValueError): font_rows(data)

    def test_strict_real_process_receipt(self):
        stdout = cpu_fixture()
        receipt = {"exit": 0, "reaped": True, "pipe_capture_complete": True,
                   "timed_out": False, "output_overflow": False, "reader_errors": {},
                   "stdout_bytes": len(stdout), "stderr_bytes": 0, "command": [sys.executable]}
        completed(receipt, stdout, b"", Path(sys.executable))
        changes = (("exit", False), ("exit", 1), ("exit", None), ("reaped", False),
                   ("pipe_capture_complete", False), ("timed_out", True), ("output_overflow", True),
                   ("reader_errors", None), ("reader_errors", {"stdout": ["writer failure"]}),
                   ("stdout_bytes", len(stdout) - 1), ("stderr_bytes", 1),
                   ("command", []), ("command", [str(Path(__file__).resolve())]))
        for key, value in changes:
            changed = copy.deepcopy(receipt)
            changed[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                completed(changed, stdout, b"", Path(sys.executable))
        with self.assertRaises(ValueError): completed(receipt, stdout, b"unexpected stderr", Path(sys.executable))

    def test_original_origin_and_activation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            jar = root / "original.jar"
            jar.write_bytes(b"pure validator fixture; not a game archive")
            helpers = root / "classes"
            helpers.mkdir()
            rows = ["[0.001s][info][class,load] GeNwCpuReference source: " + helpers.as_uri(),
                    "[0.002s][info][class,load] ge source: " + jar.as_uri(),
                    "[0.003s][info][class,load] nw source: " + jar.as_uri(),
                    "[0.004s][info][class,init] 1 Initializing 'ge' (0x1)",
                    "[0.005s][info][class,init] 2 Initializing 'nw' (0x2)"]
            data = ("\n".join(rows) + "\n").encode()
            activation(data, jar, {"ge", "nw"}, helpers)
            negatives = [data.replace(jar.as_uri().encode(), jar.as_uri().encode() + b"?q=1", 1),
                         data.replace(jar.as_uri().encode(), jar.as_uri().encode() + b"#fragment", 1),
                         ("\n".join(rows[1:]) + "\n").encode(),
                         data + (rows[1] + "\n").encode(),
                         data + b"[0.006s][info][class,init] 3 Initializing 'org/lwjgl/Sys' (0x3)\n",
                         data + b"unrelated exception diagnostics\n"]
            for altered in negatives:
                with self.subTest(data=altered[-70:]), self.assertRaises(ValueError):
                    activation(altered, jar, {"ge", "nw"}, helpers)


if __name__ == "__main__":
    unittest.main()
