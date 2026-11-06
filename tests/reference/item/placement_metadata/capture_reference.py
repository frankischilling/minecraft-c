"""Repeat the unchanged-original placement-metadata scalar observation twice."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = (
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--jdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    if digest(jar) != JAR:
        raise ValueError("verified original JAR")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    products = out / "classes"
    products.mkdir()
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    suffix = ".exe" if os.name == "nt" else ""
    java = (args.jdk / "bin" / ("java" + suffix)).resolve(strict=True)
    javac = (args.jdk / "bin" / ("javac" + suffix)).resolve(strict=True)
    jvm = args.jdk / ("bin/server/jvm.dll" if os.name == "nt" else "lib/server/libjvm.so")
    reader = ROOT / "tests/reference/client/controller/classfile.py"
    inputs = {"original/client.jar": jar, **{
        "libraries/" + name: path for name, path in zip(LIBRARIES, libraries)},
        "jdk/java": java, "jdk/javac": javac,
        "jdk/modules": (args.jdk / "lib/modules").resolve(strict=True),
        "jdk/jvm": jvm.resolve(strict=True), "tools/classfile.py": reader,
        "inputs/registered-item-classes.tsv": ROOT / "tests/reference/item/mining/classes.tsv"}
    for name in ("PlacementMetadataObservation.java", "operands.txt", "declared-inputs.json", "capture_reference.py"):
        inputs["authored/" + name] = HERE / name
    before = {name: digest(path) for name, path in inputs.items()}
    spec = importlib.util.spec_from_file_location("placement_classfile", reader)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    receivers = sorted({line.split("\t")[2] for line in
                        (ROOT / "tests/reference/item/mining/classes.tsv").read_text().splitlines()})
    pins = {"jar_sha256": JAR, "receiver_classes": receivers, "classes": {}}
    selected = {("b", "(I)I"), ("<clinit>", "()V")}
    with zipfile.ZipFile(jar) as archive:
        for name in sorted(set(receivers) | {"uu", "iz"}):
            info = module.inspect_class(archive.read(name + ".class"))
            info["methods"] = [method for method in info["methods"] if
                               (method["name"], method["descriptor"]) in selected or
                               (name == "ck" and method["name"] == "a" and method["descriptor"] == "(Liz;Lgs;Lfd;IIII)Z") or
                               (name == "iz" and method["name"] == "h" and method["descriptor"] == "()I")]
            pins["classes"][name] = info
    write_json(out / "source-pins.json", pins)
    inputs["authored/source-pins.json"] = out / "source-pins.json"
    before["authored/source-pins.json"] = digest(out / "source-pins.json")
    environment = os.environ.copy()
    removed = [name for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS") if environment.pop(name, None) is not None]
    classpath = os.pathsep.join(str(path) for path in [products, jar, *libraries])
    compile_command = [str(javac), "-encoding", "UTF-8", "-implicit:none", "-cp", classpath,
                       "-d", str(products), str(HERE / "PlacementMetadataObservation.java")]
    command = [str(java), "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true",
               "-Djava.security.manager=allow", "-cp", classpath, "PlacementMetadataObservation",
               str(jar), str(HERE / "operands.txt")]
    write_json(out / "freeze.json", {"inputs": before, "exact_paths": {name: str(path) for name, path in inputs.items()},
                                     "compile_argv": compile_command, "ordinary_argv": command,
                                     "removed_environment_names": removed})

    def run(argv, stem):
        result = subprocess.run(argv, env=environment, capture_output=True, timeout=120)
        (out / (stem + ".stdout")).write_bytes(result.stdout)
        (out / (stem + ".stderr")).write_bytes(result.stderr)
        record = {"returncode": result.returncode, "stdout_sha256": digest(out / (stem + ".stdout")),
                  "stderr_raw_private_sha256": digest(out / (stem + ".stderr"))}
        write_json(out / (stem + ".json"), record)
        return record

    compiled = run(compile_command, "compile")
    if compiled["returncode"] != 0:
        raise ValueError("authored helper compile failed; artifacts retained")
    helpers = {path.name: digest(path) for path in sorted(products.glob("*.class"))}
    write_json(out / "helper-freeze.json", helpers)
    runs = [run(command, "ordinary-" + str(index)) for index in range(2)]
    after = {name: digest(path) for name, path in inputs.items()}
    helpers_after = {path.name: digest(path) for path in sorted(products.glob("*.class"))}
    raw = (out / "ordinary-0.stdout").read_bytes()
    projection = [line for line in raw.decode("ascii").splitlines()
                  if line.startswith(("CLASS\t", "METADATA\t", "COMPLETE\t"))]
    (out / "original.stdout").write_bytes(("\n".join(projection) + "\n").encode("ascii"))
    passed = (all(run["returncode"] == 0 for run in runs) and before == after and helpers == helpers_after
              and raw == (out / "ordinary-1.stdout").read_bytes()
              and len(projection) == 4243 and projection[-1] == "COMPLETE\t202\t38\t4040")
    receipt = {"status": "PASS" if passed else "FAIL", "ordinary_fresh_vms": 2,
               "original_game_class_replacements": 0, "inputs_before": before, "inputs_after": after,
               "inputs_unchanged": before == after, "helpers_before": helpers, "helpers_after": helpers_after,
               "helpers_unchanged": helpers == helpers_after, "compile": compiled, "runs": runs,
               "ordinary_vm_flags": command[1:5], "removed_environment_names": removed,
               "raw_outputs_identical": raw == (out / "ordinary-1.stdout").read_bytes(),
               "original_stdout_sha256": digest(out / "original.stdout"),
               "projection": "Portable artifact identities replace absolute paths. ordinary-*.stdout are exact captured bytes. original.stdout selects CLASS/METADATA/COMPLETE ASCII lines and writes LF. Private raw stderr is represented by exact hashes; no stderr text is published.",
               "private_freeze_sha256": digest(out / "freeze.json"),
               "private_helper_freeze_sha256": digest(out / "helper-freeze.json")}
    write_json(out / "capture-provenance.json", receipt)
    print(receipt["status"], "two fresh unchanged-original VMs;", len(projection), "selected lines;", out)
    if not passed:
        raise ValueError("capture failed; all outcomes retained")


if __name__ == "__main__":
    main()
