"""Compare native World rays with untouched-client reflection observations.

All reference bytes, generated classfiles, stdout and logs remain in the
explicitly supplied new private work directory. No JVM is used by the client.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
JAR_SHA256 = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"


def schedule():
    rows = []

    def add(name, block=1, metadata=0, liquid=0, ignore=0, mode=0,
            start=(2.5, 64.5, 4.5), end=(6.5, 64.5, 4.5)):
        def number(value):
            return str(value) if isinstance(value, str) else float(value).hex()
        rows.append(" ".join([name, str(block), str(metadata), str(liquid), str(ignore),
                              str(mode), *(number(v) for v in (*start, *end))]))

    for block in range(1, 97):
        for metadata in (0, 5):
            add(f"block-{block}-meta-{metadata}", block, metadata)
        add(f"box-gate-{block}-meta-5", block, 5, ignore=1)
    for axis in range(3):
        for negative in (False, True):
            start, end = [4.5, 64.5, 4.5], [4.5, 64.5, 4.5]
            start[axis] -= 2 if not negative else -2
            end[axis] += 2 if not negative else -2
            add(f"axis-{axis}-negative-{int(negative)}", start=start, end=end)
    add("inside", start=(4.5, 64.5, 4.5))
    add("xy-tie", start=(3.5, 63.5, 4.5), end=(4.5, 64.5, 4.5))
    add("xyz-tie", start=(3.5, 63.5, 3.5), end=(4.5, 64.5, 4.5))
    add("negative-boundary", start=(6., 64.5, 4.5), end=(2., 64.5, 4.5))
    add("same-cell-air", block=0, start=(4.5, 64.5, 4.5), end=(4.5, 64.5, 4.5))
    add("zero-length-inside", start=(4.5, 64.5, 4.5), end=(4.5, 64.5, 4.5))
    add("sub-epsilon-inside", start=(4.5, 64.5, 4.5), end=(4.5001, 64.5001, 4.5001))
    add("null-start", mode=2)
    add("null-end", mode=3)
    add("nan-shortcircuit-null-end", mode=3, start=("NaN", 64.5, 4.5))
    add("end-nan", end=(6.5, "NaN", 4.5))
    add("unregistered-positive-id", block=255)
    add("201-steps", block=0, start=(.5, 129.5, .5), end=(1000.5, 129.5, .5))
    add("positive-infinity", block=0, start=(.5, 129.5, .5), end=("Infinity", 129.5, .5))
    add("negative-infinity-floor-wrap", block=0, start=(.5, 129.5, .5),
        end=("-Infinity", 129.5, .5))
    for block in (8, 9, 10, 11):
        for metadata in (0, 1, 8):
            for liquid, ignore in ((0, 0), (1, 0), (1, 1)):
                add(f"fluid-{block}-{metadata}-{liquid}-{ignore}", block, metadata, liquid, ignore)
    for block in (50, 75, 76):
        for metadata in range(6):
            add(f"torch-{block}-{metadata}", block, metadata, start=(2.5, 64.25, 4.5))
    add("pool-alias-stone", mode=1)
    add("pool-alias-tie", mode=1, start=(3.5, 63.5, 3.5), end=(4.5, 64.5, 4.5))
    return "\n".join(rows) + "\n"


def run(command, work, name, data):
    result = subprocess.run(command, input=data, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=180)
    (work / f"{name}.stdout").write_bytes(result.stdout)
    (work / f"{name}.stderr").write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{name} failed: {result.returncode}; see private stderr")
    return [line for line in result.stdout.decode().splitlines() if line.startswith("RAY\t")]


def main():
    from run_raytrace import encode_native_inputs
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", required=True, type=Path)
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    args = parser.parse_args()
    jar, native = args.jar.resolve(strict=True), args.native.resolve(strict=True)
    if hashlib.sha256(jar.read_bytes()).hexdigest() != JAR_SHA256:
        raise ValueError("untouched client JAR hash mismatch")
    work = args.work_dir.resolve()
    work.mkdir()
    classes = work / "classes"
    classes.mkdir()
    sources = [ROOT / "tests/reference/world/MutationObservation.java",
               ROOT / "tests/reference/world/RaytraceObservation.java"]
    subprocess.run([args.javac, "-encoding", "UTF-8", "-d", str(classes),
                    *(str(path) for path in sources)], check=True, timeout=60)
    data = schedule().encode("ascii")
    (work / "prepared-input.tsv").write_bytes(data)
    original = run([args.java, "-Xint", "-Xmx256m", "--add-opens=java.base/java.util=ALL-UNNAMED",
                    "--add-opens=java.base/java.util.concurrent.atomic=ALL-UNNAMED",
                    "-cp", str(classes), "RaytraceObservation", str(jar),
                    str(work / "original-world")], work, "original", data)
    native_root = work / "native-world"
    native_root.mkdir()
    actual = run([str(native), "world-raytrace-probe", str(native_root).encode().hex()],
                 work, "native", encode_native_inputs(data))
    expected_count = len(data.splitlines())
    if len(original) != expected_count or len(actual) != expected_count:
        raise AssertionError(f"incomplete rows: original={len(original)} native={len(actual)}")
    mismatches = [{"index": index, "original": left, "native": right}
                  for index, (left, right) in enumerate(zip(original, actual)) if left != right]
    report = {"jar_sha256": JAR_SHA256, "rows": expected_count, "mismatches": mismatches,
              "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in sources},
              "scope": "real original cached World/normal Chunk, prepared cell/bounds, complete ray result/start/pool cursor/bounds"}
    (work / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if mismatches:
        raise AssertionError(f"{len(mismatches)} exact ray mismatches; see {work / 'comparison.json'}")
    print(f"PASS: {expected_count} exact untouched-client World ray rows")


if __name__ == "__main__":
    main()
