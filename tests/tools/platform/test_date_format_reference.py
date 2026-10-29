"""Compare the native DateFormat owner with the selected real JDK17 constructor."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "tests/reference/platform/date_format/DateFormatObservation.java"
VECTORS = ROOT / "tests/reference/platform/date_format/vectors.tsv"


def pin(path):
    value = path.read_bytes()
    return {"bytes": len(value), "sha256": hashlib.sha256(value).hexdigest()}


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("java", "javac", "native", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    paths = [args.java.resolve(strict=True), args.javac.resolve(strict=True),
             args.native.resolve(strict=True), SOURCE, VECTORS]
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="date-format-", dir=args.work_dir.resolve(strict=True)))
    report = {"inputs": {str(p): pin(p) for p in paths}, "processes": [], "passed": False}

    def run(name, command):
        result = subprocess.run([str(x) for x in command], capture_output=True, timeout=60)
        (work / (name + ".stdout")).write_bytes(result.stdout)
        (work / (name + ".stderr")).write_bytes(result.stderr)
        report["processes"].append({"name": name, "argv": [str(x) for x in command],
                                    "exit": result.returncode})
        require(result.returncode == 0, name + " completed exit0")
        return result.stdout.decode("utf-8"), result.stderr.decode("utf-8")

    try:
        out, err = run("jdk-version", [paths[0], "-version"])
        require(re.search(r'version "17[.]', out + err) is not None, "actual selected JDK17")
        report["jdk"] = out + err
        run("compile", [paths[1], "-encoding", "UTF-8", "-d", work, SOURCE])
        vector_output, vector_error = run("vectors", [paths[0], "-cp", work,
                                                       "DateFormatObservation", "--vectors"])
        require(vector_error == "", "no reference diagnostic")
        actual = [x for x in vector_output.splitlines() if not x.startswith("#")]
        recorded = [x for x in VECTORS.read_text().splitlines() if not x.startswith("#")]
        require(actual == recorded and len(actual) == 2926, "all recorded actual JDK vectors")
        report["vectors"] = len(actual)
        native_out, native_err = run("native-vectors", [paths[2], VECTORS])
        require(native_err == "" and "2926 JDK vectors" in native_out,
                "native complete public owner regression")
        values = ["-1", "0", "-94694400000", "126687600000", "162370800000",
                  "1710053999999", "1710054000000", "1730613599999", "1730613600000",
                  "1735707599999", "1735707600000", "2147483648000", "4102444799999"]
        original, original_error = run("default-reference", [paths[0], "-cp", work,
                                                               "DateFormatObservation", *values])
        native, native_error = run("default-native", [paths[2], "--observe", *values])
        require(original_error == native_error == "", "no default format diagnostic")
        aliases = {"Etc/UTC": "UTC", "Etc/GMT": "UTC", "GMT": "UTC",
                   "US/Eastern": "America/New_York"}

        def records(text):
            value = dict(line.split("=", 1) for line in text.splitlines())
            value["timezone"] = aliases.get(value["timezone"], value["timezone"])
            return value

        require(records(original) == records(native), "default constructor/profile/UTF16 results")
        report["default_profile"] = records(original)
        require(report["inputs"] == {str(p): pin(p) for p in paths}, "inputs unchanged")
        report["passed"] = True
    finally:
        (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8",
                                           newline="\n")
        print("DateFormat artifacts: " + str(work))
    print("PASS: real JDK17 default capture and native UTF16 formatter; 2926 transition vectors")


if __name__ == "__main__":
    main()
