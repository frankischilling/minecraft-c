"""Two fresh unchanged-original plant placement replays on genuine saved Worlds."""

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


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def inventory(path):
    return {p.relative_to(path).as_posix(): digest(p) for p in sorted(path.rglob("*")) if p.is_file()}


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
    support_path = ROOT / "tests/reference/client/picking/observe.py"
    support = load("plant_world_support", support_path)
    reader_path = ROOT / "tests/reference/client/controller/classfile.py"
    reader = load("plant_classfile", reader_path)
    hierarchy_path = ROOT / "tests/reference/client/placement/verify.py"
    hierarchy = load("plant_static_classes", hierarchy_path)
    static_path = ROOT / "tests/reference/client/placement/source-pins.json"
    static = json.loads(static_path.read_text(encoding="utf-8"))
    libraries = [(args.libraries / name).resolve(strict=True) for name in support.LIBRARIES]
    if any(digest(path) != expected for path, expected in zip(libraries, support.LIBRARIES.values())):
        raise ValueError("pinned original SDK dependencies")
    suffix = ".exe" if os.name == "nt" else ""
    java = (args.jdk / "bin" / ("java" + suffix)).resolve(strict=True)
    javac = (args.jdk / "bin" / ("javac" + suffix)).resolve(strict=True)
    jvm = args.jdk / ("bin/server/jvm.dll" if os.name == "nt" else "lib/server/libjvm.so")
    inputs = {"original/client.jar": jar,
              **{"libraries/" + name: path for name, path in zip(support.LIBRARIES, libraries)},
              "jdk/java": java, "jdk/javac": javac,
              "jdk/modules": (args.jdk / "lib/modules").resolve(strict=True),
              "jdk/jvm": jvm.resolve(strict=True), "tools/classfile.py": reader_path,
              "tools/placement-static-verify.py": hierarchy_path,
              "inputs/placement-static-pins.json": static_path,
              "tools/world-inputs.py": support_path,
              "tools/region-inputs.py": ROOT / "tests/tools/client/test_picking.py"}
    for name in ("PlantPlacementObservation.java", "profiles.tsv", "declared-inputs.json", "capture_reference.py"):
        inputs["authored/" + name] = HERE / name
    pins = {"jar_sha256": JAR, "placement_static_packet_sha256": digest(static_path), "classes": {}}
    selected = {"fd", "lm", "mx", "ok", "wi", "xa", "xp", "uu", "wb", "he", "ru", "jb", "tl", "ni", "ln"}
    with zipfile.ZipFile(jar) as archive:
        values = {name[:-6]: archive.read(name) for name in archive.namelist() if name.endswith(".class")}
        parents = {name: hierarchy.ClassReader(data).parent for name, data in values.items()}
        for name in parents:
            current = name
            while current:
                if current == "ln":
                    selected.add(name)
                    break
                current = parents.get(current)
        for name in sorted(selected):
            info = reader.inspect_class(values[name])
            if name in static["classes"] and info["class_sha256"] != static["classes"][name]["class_sha256"]:
                raise ValueError("current original/static class continuity: " + name)
            pins["classes"][name] = {**info, "super_class": parents[name]}
    write(out / "source-pins.json", pins)
    inputs["authored/source-pins.json"] = out / "source-pins.json"
    before = {name: digest(path) for name, path in inputs.items()}
    worlds = []
    world_inputs = []
    for index in range(2):
        world = out / ("ordinary-" + str(index)) / "world"
        support.world(world)
        worlds.append(world)
        world_inputs.append(inventory(world))
    if world_inputs[0] != world_inputs[1]:
        raise ValueError("identical frozen authored World inputs")
    environment = os.environ.copy()
    removed = [name for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS") if environment.pop(name, None) is not None]
    classpath = os.pathsep.join(map(str, [products, jar, *libraries]))
    compile_command = [str(javac), "-encoding", "UTF-8", "-implicit:none", "-cp", classpath,
                       "-d", str(products), str(HERE / "PlantPlacementObservation.java")]
    flags = ["-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
             "--add-opens", "java.base/java.util=ALL-UNNAMED"]
    commands = [[str(java), *flags, "-cp", classpath, "PlantPlacementObservation", str(jar),
                 str(world), str(HERE / "profiles.tsv")] for world in worlds]
    write(out / "freeze.json", {"inputs": before, "exact_paths": {name: str(path) for name, path in inputs.items()},
                                 "world_inputs": world_inputs, "compile_argv": compile_command,
                                 "ordinary_argv": commands, "removed_environment_names": removed})

    def run(argv, stem):
        try:
            result = subprocess.run(argv, env=environment, capture_output=True, timeout=120)
            code, stdout, stderr = result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired as error:
            code, stdout, stderr = -1, error.stdout or b"", error.stderr or b""
        (out / (stem + ".stdout")).write_bytes(stdout)
        (out / (stem + ".stderr")).write_bytes(stderr)
        record = {"returncode": code, "stdout_sha256": digest(out / (stem + ".stdout")),
                  "stderr_raw_private_sha256": digest(out / (stem + ".stderr"))}
        write(out / (stem + ".json"), record)
        return record

    compiled = run(compile_command, "compile")
    if compiled["returncode"] != 0:
        raise ValueError("authored helper compile failed; exact artifacts retained")
    helpers = inventory(products)
    write(out / "helper-freeze.json", helpers)
    runs = [run(command, "ordinary-" + str(index)) for index, command in enumerate(commands)]
    after = {name: digest(path) for name, path in inputs.items()}
    helpers_after = inventory(products)
    world_after = [inventory(world) for world in worlds]
    world_continuity = all({name: value for name, value in prior.items() if name != "session.lock"} ==
                           {name: value for name, value in later.items() if name != "session.lock"}
                           for prior, later in zip(world_inputs, world_after))
    raw = (out / "ordinary-0.stdout").read_bytes()
    selected_lines = [line for line in raw.decode("ascii").splitlines()
                      if line.startswith(("CLASS\t", "PLANT\t", "COMPLETE\t"))]
    (out / "original.stdout").write_bytes(("\n".join(selected_lines) + "\n").encode("ascii"))
    guards = [line for line in raw.decode("ascii").splitlines() if line.startswith("PLANT_GUARD\t")]
    passed = (all(row["returncode"] == 0 for row in runs) and before == after and helpers == helpers_after
              and world_continuity and raw == (out / "ordinary-1.stdout").read_bytes()
              and len(selected_lines) == 873 and selected_lines[-1] == "COMPLETE\t8\t288\t864"
              and len(guards) == 1)
    receipt = {"status": "PASS" if passed else "FAIL", "ordinary_fresh_vms": 2,
               "original_game_class_replacements": 0, "inputs_before": before, "inputs_after": after,
               "inputs_unchanged": before == after, "helpers_before": helpers, "helpers_after": helpers_after,
               "helpers_unchanged": helpers == helpers_after, "compile": compiled, "runs": runs,
               "world_inputs_before": world_inputs, "world_files_after": world_after,
               "world_inputs_except_source_session_lock_unchanged": world_continuity,
               "ordinary_vm_flags": flags, "removed_environment_names": removed,
               "raw_outputs_identical": raw == (out / "ordinary-1.stdout").read_bytes(),
               "supporting_effect_guard": guards, "original_stdout_sha256": digest(out / "original.stdout"),
               "projection": "Portable artifact identities replace absolute paths. ordinary-*.stdout are exact raw captured bytes. original.stdout selects CLASS/PLANT/COMPLETE ASCII lines with LF. Private raw stderr hashes preserve its exact identity without private helper paths. World session.lock source writes are separately retained, not called unchanged inputs.",
               "private_freeze_sha256": digest(out / "freeze.json"),
               "private_helper_freeze_sha256": digest(out / "helper-freeze.json")}
    write(out / "capture-provenance.json", receipt)
    print(receipt["status"], "two fresh unchanged-original VMs;", len(selected_lines), "selected lines;", out)
    if not passed:
        raise ValueError("capture failed; every outcome retained")


if __name__ == "__main__":
    main()
