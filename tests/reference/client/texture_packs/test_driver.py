"""JAR/JDK/native-free controls for authored ZIP inputs and result validation."""

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ValueError("module " + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    here = Path(__file__).resolve().parent
    root = here.parents[3]
    driver = load(root / "tests/tools/client/test_texture_zip.py", "zip_driver")
    inputs = load(here / "inputs.py", "zip_inputs")
    good = [(0, b"texture ZIP: 1 checks\n", b"", 1),
            (0, b"texture ZIP: 13 checks\r\n", b"", 13),
            (0, b"texture ZIP: 12345 checks", b"", 12345)]
    bad = [(1, b"texture ZIP: 1 checks\n", b""),
           (139, b"texture ZIP: 1 checks\n", b"AddressSanitizer: crash\n"),
           (0, b"texture ZIP: 1 checks\n", b"runtime error: undefined behavior\n"),
           (0, b"texture ZIP: 1 checks\n", b"leak report\n"),
           (0, b"texture ZIP: 1 checks\nextra\n", b""),
           (0, b"texture ZIP: 1 checks\n\n", b""),
           (0, b"texture ZIP: 0 checks\n", b""),
           (0, b"texture ZIP: -1 checks\n", b""),
           (0, b"texture ZIP: +1 checks\n", b""),
           (0, b"texture ZIP: 01 checks\n", b""),
           (0, b"texture ZIP: 1 check\n", b""),
           (0, b"texture ZIP: 1 checks \n", b""),
           (0, b"texture ZIP: 1 checks\x00\n", b""),
           (0, b"texture ZIP: \xff checks\n", b""),
           (0, b"", b"")]
    for code, stdout, stderr, expected in good:
        require(driver.validate(subprocess.CompletedProcess([], code, stdout, stderr)) == expected,
                "positive result control")
    for index, (code, stdout, stderr) in enumerate(bad):
        try:
            driver.validate(subprocess.CompletedProcess([], code, stdout, stderr))
        except ValueError:
            continue
        raise ValueError("accepted malformed ZIP result " + str(index))
    with tempfile.TemporaryDirectory(prefix="texture-zip-controller-") as work:
        first, second = Path(work) / "one", Path(work) / "two"
        inputs.zip_inputs(first)
        inputs.zip_inputs(second)
        inventory = ["bad-central.zip", "bad-crc.zip", "bad-deflate.zip", "bad-local.zip",
                     "deflated.zip", "directory.zip", "duplicate.zip",
                     "exact-before-directory.zip", "stored.zip", "trailing.zip"]
        require(sorted(path.name for path in first.iterdir()) == inventory, "fixed input inventory")
        require(sorted(path.name for path in second.iterdir()) == inventory, "repeat input inventory")
        for name in inventory:
            require((first / name).read_bytes() == (second / name).read_bytes(),
                    "nondeterministic authored input " + name)
    print(json.dumps({"status": "PASS", "scope": "controller and authored inputs only; no native/JDK",
                      "result_controls": len(good) + len(bad), "authored_archives": len(inventory)}))


if __name__ == "__main__":
    main()
