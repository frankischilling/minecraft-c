"""Compare unchanged inherited Entity turning with actual native owners."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import uuid
import zipfile

sys.dont_write_bytecode = True
from classfile import inspect_class
HERE = Path(__file__).resolve().parent
sha = lambda data: hashlib.sha256(data).hexdigest()


def execute(command, directory, label, env):
    result = subprocess.run(command, cwd=directory, env=env, capture_output=True,
                            timeout=60, check=False)
    (directory / (label + ".stdout")).write_bytes(result.stdout)
    (directory / (label + ".stderr")).write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{label} exited {result.returncode}: " +
                           result.stderr.decode("utf-8", errors="replace")[-3000:])
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--java", default="java")
    parser.add_argument("--javac", default="javac")
    args = parser.parse_args()
    jar, native = args.jar.resolve(), args.native.resolve()
    provenance = json.loads((HERE / "provenance.json").read_text())
    if sha(jar.read_bytes()) != provenance["client_jar_sha256"]:
        raise ValueError("reference JAR hash differs")
    with zipfile.ZipFile(jar) as archive:
        original_names = {name[:-6].replace("/", ".") for name in archive.namelist()
                          if name.endswith(".class")}
        for name, expected in provenance["classes"].items():
            if inspect_class(archive.read(name + ".class")) != expected:
                raise ValueError("original class/method Code pin differs: " + name)
    raw_inputs = (HERE / "inputs.json").read_bytes()
    if sha(raw_inputs) != provenance["inputs_sha256"]:
        raise ValueError("authored input hash differs")
    cases = json.loads(raw_inputs)["cases"]
    if not cases or len(cases) > 10000:
        raise ValueError("case count")
    lines = []
    expected_ordinals = []
    for ordinal, case in enumerate(cases):
        if case["kind"] not in (0,1) or not 1 <= case["repeat"] <= 6:
            raise ValueError("case shape")
        values = case["angles"] + case["delta"]
        if len(values) != 6 or any(not 0 <= value <= 0xffffffff for value in values):
            raise ValueError("binary32 input range")
        lines.append(str(case["kind"]) + " " + " ".join(f"{value:08x}" for value in values) +
                     " " + str(case["repeat"]))
        expected_ordinals.extend((ordinal,step) for step in range(case["repeat"]))
    output = args.work_dir.resolve() / ("run-" + uuid.uuid4().hex)
    output.mkdir(parents=True)
    classes = output / "probe-classes"
    classes.mkdir()
    plan = output / "inputs.plan"
    plan.write_bytes(("\n".join(lines) + "\n").encode("ascii"))
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS"):
        env.pop(key, None)
    execute([args.javac,"-classpath",str(jar),"-d",str(classes),
             str(HERE / "EntityTurnProbe.java")],output,"javac",env)
    original = execute([args.java,"-Xverify:all","--add-opens=java.base/java.util=ALL-UNNAMED",
                        "-Xlog:class+init=info:file=original-class-init.log",
                        "-classpath",str(classes) + os.pathsep + str(jar),
                        "EntityTurnProbe",str(plan)],output,"original",env)
    initialized = set()
    for line in (output / "original-class-init.log").read_text().splitlines():
        match = re.search(r"Initializing '([^']+)'",line)
        if match and match.group(1).replace("/", ".") in original_names:
            initialized.add(match.group(1).replace("/", "."))
    allowed = {"sn","hl","ju","eq","ud","ma"}
    if initialized != allowed:
        raise ValueError("unexpected original class initialization: " + repr(initialized))
    actual = execute([str(native),str(plan)],output,"native",env)
    expected_lines, actual_lines = original.splitlines(), actual.splitlines()
    if len(expected_lines) != len(expected_ordinals) or len(actual_lines) != len(expected_ordinals):
        raise ValueError("observation record count")
    pattern = re.compile(rb"([0-9]+) ([0-9]+) [0-9a-f]{8} [0-9a-f]{8} [0-9a-f]{8} [0-9a-f]{8} 1")
    mismatches = []
    for index,(expected,observed) in enumerate(zip(expected_lines,actual_lines)):
        match = pattern.fullmatch(expected)
        if not match or (int(match[1]),int(match[2])) != expected_ordinals[index] or not pattern.fullmatch(observed):
            raise ValueError("malformed observation record")
        if expected != observed:
            mismatches.append({"index":index,"original":expected.decode(),"native":observed.decode()})
    report = {"result":"exact match" if not mismatches else "mismatch", "cases":len(cases),
              "records":len(expected_ordinals),"projected_fields_per_record":5,
              "projected_fields":len(expected_ordinals)*5,"mismatches":mismatches,
              "client_jar_sha256":provenance["client_jar_sha256"],"inputs_sha256":sha(raw_inputs),
              "plan_sha256":sha(plan.read_bytes()),"native_executable_sha256":sha(native.read_bytes()),
              "original_initialized_classes":sorted(initialized),"original_game_classes_modified":False,
              "receiver_constructors":["hl.<init>(Lfd;)V","ju.<init>(Lfd;)V"],"world_argument":None,
              "reference_helpers":{name:sha((HERE/name).read_bytes()) for name in
                                   ("classfile.py","compare.py","inputs.json","EntityTurnProbe.java","provenance.json")},
              "original_stdout_sha256":sha(original),"native_stdout_sha256":sha(actual)}
    (output / "result.json").write_text(json.dumps(report,indent=2) + "\n")
    print(json.dumps({"result":report["result"],"records":report["records"],"mismatches":len(mismatches),"output":str(output)}))
    if mismatches:
        print(json.dumps(mismatches[:8]))
        raise SystemExit(1)


if __name__ == "__main__":
    main()
