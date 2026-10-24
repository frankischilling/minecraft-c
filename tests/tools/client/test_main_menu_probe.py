"""Exercise native scene admission before resource or graphics initialization."""
import argparse
from pathlib import Path
import subprocess
import tempfile


BASE = ("MC_GUI_MAIN_MENU_SCENE_V1\n"
        "display 854 480\n"
        "gui-scale 0\n"
        "mouse 0 0\n"
        "partial-tick 00000000\n"
        "clock-millis 0\n"
        "splash 1 0041\n"
        "buttons 1\n"
        "button 4 0 0 98 20 1 1 1 0042\n"
        "end\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    valid = {
        "ordinary": BASE,
        "zero_buttons": BASE.replace("buttons 1\nbutton 4 0 0 98 20 1 1 1 0042\n", "buttons 0\n"),
        "null_strings": BASE.replace("splash 1 0041", "splash -1").replace("1 1 1 0042", "1 1 -1"),
        "empty_strings": BASE.replace("splash 1 0041", "splash 0").replace("1 1 1 0042", "1 1 0"),
        "counted_units": BASE.replace("splash 1 0041", "splash 3 0000 d800 ffff"),
        "long_min": BASE.replace("clock-millis 0", "clock-millis -9223372036854775808"),
        "long_max": BASE.replace("clock-millis 0", "clock-millis 9223372036854775807"),
        "int_limits": BASE.replace("gui-scale 0", "gui-scale -2147483648")
                          .replace("mouse 0 0", "mouse -2147483648 2147483647")
                          .replace("button 4 0 0 98 20", "button -2147483648 2147483647 -2147483648 -1 0"),
        "hidden_disabled": BASE.replace("98 20 1 1", "98 20 0 0"),
        "maximum_pixels": BASE.replace("display 854 480", "display 4096 1024"),
        "minimum_pixels": BASE.replace("display 854 480", "display 1 1"),
    }
    for bits in ("7f800000", "ff800000", "7fc00000", "7f800001", "ffffffff", "80000000"):
        valid["partial_" + bits] = BASE.replace("partial-tick 00000000", "partial-tick " + bits)
    button = "button 4 0 0 98 20 1 1 -1\n"
    valid["maximum_buttons"] = BASE[:BASE.index("buttons 1\n")] + "buttons 32\n" + button * 32 + "end\n"
    text = "4096 " + " ".join(["0041"] * 4096)
    valid["large_counted_text"] = (BASE[:BASE.index("splash 1 0041\n")] + "splash " + text +
                                    "\nbuttons 2\n" + ("button 4 0 0 98 20 1 1 " + text + "\n") * 2 + "end\n")
    invalid = {
        "empty": "", "bad_header": BASE.replace("SCENE_V1", "SCENE_V2"),
        "crlf": BASE.replace("\n", "\r\n"), "missing_lf": BASE[:-1],
        "trailing_line": BASE + "\n", "trailing_token": BASE.replace("end\n", "end extra\n"),
        "nul_byte": BASE.replace("gui-scale", "gui\0scale"),
        "double_space": BASE.replace("display 854", "display  854"),
        "leading_space": " " + BASE, "tab": BASE.replace("mouse 0", "mouse\t0"),
        "leading_zero": BASE.replace("display 854", "display 0854"),
        "plus_sign": BASE.replace("clock-millis 0", "clock-millis +0"),
        "negative_zero": BASE.replace("clock-millis 0", "clock-millis -0"),
        "long_overflow": BASE.replace("clock-millis 0", "clock-millis 9223372036854775808"),
        "long_underflow": BASE.replace("clock-millis 0", "clock-millis -9223372036854775809"),
        "int_overflow": BASE.replace("mouse 0 0", "mouse 2147483648 0"),
        "zero_width": BASE.replace("display 854 480", "display 0 480"),
        "axis_limit": BASE.replace("display 854 480", "display 4097 1"),
        "pixel_limit": BASE.replace("display 854 480", "display 4096 1025"),
        "short_bits": BASE.replace("partial-tick 00000000", "partial-tick 0000000"),
        "uppercase_bits": BASE.replace("partial-tick 00000000", "partial-tick 7FC00000"),
        "bad_unit": BASE.replace("splash 1 0041", "splash 1 zzzz"),
        "short_unit": BASE.replace("splash 1 0041", "splash 1 041"),
        "missing_unit": BASE.replace("splash 1 0041", "splash 1"),
        "extra_unit": BASE.replace("splash 1 0041", "splash 0 0041"),
        "null_with_unit": BASE.replace("splash 1 0041", "splash -1 0041"),
        "negative_length": BASE.replace("splash 1 0041", "splash -2"),
        "string_limit": BASE.replace("splash 1 0041", "splash 4097"),
        "button_limit": BASE.replace("buttons 1", "buttons 33"),
        "negative_buttons": BASE.replace("buttons 1", "buttons -1"),
        "enabled_flag": BASE.replace("98 20 1 1", "98 20 2 1"),
        "visible_flag": BASE.replace("98 20 1 1", "98 20 1 -1"),
        "missing_button": BASE.replace("button 4 0 0 98 20 1 1 1 0042\n", ""),
        "file_limit": BASE + "x" * (65537 - len(BASE)),
    }
    # Empty resource root is rejected by the real FileSource constructor before
    # any Registry/display creation. Positive cases prove parser admission only.
    prefix = b"invalid or unreadable bounded GuiMainMenu scene"
    with tempfile.TemporaryDirectory(prefix="mc-main-menu-admission-") as temporary:
        directory = Path(temporary)
        for admitted, cases in ((True, valid), (False, invalid)):
            for name, source in cases.items():
                path = directory / (name + ".scene")
                path.write_bytes(source.encode("ascii"))
                result = subprocess.run([str(executable), "", str(path)], cwd=directory,
                                        capture_output=True, timeout=10, check=False)
                assert result.returncode == 1 and result.stdout == b"", (name, result.returncode, result.stdout[:100])
                lines = result.stderr.splitlines()
                assert len(lines) == (1 if admitted else 2), (name, result.stderr)
                assert (lines[0] == prefix) != admitted, (name, result.stderr)
                assert lines[-1].startswith(b"GuiMainMenu probe failed:"), (name, result.stderr)
    print(f"PASS: {len(valid)} admitted and {len(invalid)} rejected native GuiMainMenu scenes; no graphics initialization")


if __name__ == "__main__":
    main()
