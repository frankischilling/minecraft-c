"""Optional original-JAR LS view observation; no ordinary test/runtime JVM edge."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

JAR_SHA256 = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"


def fbits(value):
    return struct.unpack(">I", struct.pack(">f", value))[0]


def dbits(value):
    return struct.unpack(">Q", struct.pack(">d", value))[0]


def cases():
    rows = []
    partials = [0x3F800000, 0, 0x80000000, 0x3F000000, 0x3F7FFFFF, 0x3F800001,
                0xBF800000, 0x3FC00000, 0x7FC12345, 0x7F800000, 0xFF800000, 1, 0x7F812345]
    yaws = [fbits(x) for x in (0, 1, -180, 350.5, 720.25, -450.125, 89.99, -0.0)]
    pitches = [fbits(x) for x in (-90, -20.25, 0, 45.5, 90)]
    ordinary = [dbits(x) for x in (1.25, 64.5, -3.75, -4.5, 63.25, 10.5)]
    for yindex, yaw in enumerate(yaws):
        for pindex, partial in enumerate(partials):
            rows.append((partial, ordinary, yaw, pitches[(yindex + pindex) % len(pitches)],
                         fbits(350), fbits(-37.125)))
    for angle in (0x7FC12345, 0xFFC54321, 0x7F812345, 0x7F800000, 0xFF800000, 1):
        for partial in partials[:8]:
            rows.append((partial, ordinary, angle, angle, fbits(10), fbits(11)))
    special = [0, 0x8000000000000000, 0x7FF8000000001234, 0xFFF8000000004567,
               0x7FF0000000001234, 0x7FF0000000000000, 0xFFF0000000000000,
               0x7FEFFFFFFFFFFFFF, 1, 0x8000000000000001]
    for index, value in enumerate(special):
        coordinates = [value, special[(index + 1) % len(special)], dbits(-0.0),
                       special[(index + 3) % len(special)], value, dbits(0)]
        for partial in (0x3F800000, 0, 0x3F000000, 0x7FC12345):
            rows.append((partial, coordinates, fbits(45), fbits(-30), fbits(-405), fbits(60)))
    return [str(i) + " " + f"{p:08x} " + " ".join(f"{v:016x}" for v in xyz) + " " +
            " ".join(f"{v:08x}" for v in angles)
            for i, (p, xyz, *angles) in enumerate(rows)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    if hashlib.sha256(jar.read_bytes()).hexdigest() != JAR_SHA256:
        raise RuntimeError("original client JAR hash mismatch")
    work = args.work_dir.resolve()
    try:
        work.mkdir(parents=True)
    except FileExistsError:
        parser.error(f"--work-dir must name a fresh directory: {work} already exists; "
                     "existing files are left untouched and no compiler/JVM is launched")
    source = Path(__file__).with_name("ViewObservation.java").resolve()
    inputs = work / "inputs.tsv"
    inputs.write_bytes(("\n".join(cases()) + "\n").encode("ascii"))
    compile_command = [args.javac, "-cp", str(jar), "-d", str(work), str(source)]
    subprocess.run(compile_command, check=True, capture_output=True)
    import os
    command = [args.java, "-Xint", "-cp", os.pathsep.join((str(work), str(jar))),
               "ViewObservation", str(inputs)]
    run = subprocess.run(command, capture_output=True, timeout=60, check=True)
    output = run.stdout.replace(b"\r\n", b"\n")
    if run.stderr or len(output.splitlines()) != len(cases()):
        raise RuntimeError("unexpected original observation output")
    (work / "vectors.tsv").write_bytes(output)
    report = {"scope": "Authored receiver fields; actual original LS.e/f/ac + IN/BT bodies only",
              "jar_sha256": JAR_SHA256, "compile_command": compile_command, "command": command,
              "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
              "input_sha256": hashlib.sha256(inputs.read_bytes()).hexdigest(),
              "output_sha256": hashlib.sha256(output).hexdigest(), "cases": len(cases()),
              "jdk": subprocess.run([args.java, "-version"], capture_output=True,
                                    check=True).stderr.decode().splitlines()}
    (work / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
