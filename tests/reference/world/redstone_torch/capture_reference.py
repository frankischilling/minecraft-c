"""Eight fresh unchanged-original captures with bounded observed-stream accounting."""
import argparse
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
EFFECT_KEYS = set("cohort index name values world_role time result exception center_before center_after arrays_before arrays_after changes worlds_before worlds_after history_before history_after position_set_before position_set_after qy_before qy_after events drops observer_refused".split())
ENTROPY_KEYS = set("cohort index name mode owners_before owners_after supplied_alias math_before math_after seed_uniquifier_before seed_uniquifier_after events".split())
EXCEPTION_KEYS = set("cohort index name exception frames declared_callback".split())


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


O = module("redstone_torch_output", HERE / "observation.py")
A = module("redstone_torch_accounting", HERE / "accounting.py")
T = module("redstone_torch_topology", HERE / "topology.py")
decode = O.decode


def require(value, message):
    if not value:
        raise AssertionError(message)


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="ascii", newline="\n")


def inventory(path):
    return {p.relative_to(path).as_posix(): C.digest(p)
            for p in sorted(path.rglob("*")) if p.is_file()}


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def loads(value):
    def invalid(value):
        raise ValueError("non-JSON constant " + value)
    return json.loads(value, object_pairs_hook=unique, parse_constant=invalid)


def same(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return set(a) == set(b) and all(same(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    return a == b



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--jar", type=Path, default=ROOT / "b1.7.3.jar")
    parser.add_argument("--libraries", type=Path, default=Path(os.environ.get("APPDATA", "")) / ".minecraft/libraries")
    parser.add_argument("--java", default=shutil.which("java"))
    parser.add_argument("--javac", default=shutil.which("javac"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    require(args.execute, "capture requires explicit --execute")
    require(HERE.relative_to(ROOT).as_posix()=="tests/reference/world/redstone_torch",
            "fresh capture must use the proposed public input/helper destination")
    global C,I,P
    C=module("db_capture",ROOT/"tests/reference/client/controller/capture.py")
    I=module("db_classfile",ROOT/"tests/reference/client/controller/classfile.py")
    P=module("db_libraries",ROOT/"tests/reference/client/picking/observe.py")
    jar = args.jar.resolve(strict=True)
    require(C.digest(jar) == JAR, "verified unchanged original JAR")
    java, javac = [Path(shutil.which(p) or p).resolve(strict=True) for p in (args.java, args.javac)]
    require(java.parent == javac.parent, "one actual JDK")
    jdk = java.parent.parent
    jvm = java.parent / "server/jvm.dll"
    if not jvm.is_file(): jvm = jdk / "lib/server/libjvm.so"
    jmod = jdk / "jmods/java.base.jmod"
    unsupported = jdk / "jmods/jdk.unsupported.jmod"
    libraries = [(args.libraries / name).resolve(strict=True) for name in P.LIBRARIES]
    require(all(C.digest(p) == pin for p, pin in zip(libraries, P.LIBRARIES.values())), "pinned SDK libraries")
    frozen = loads((HERE / "input-freeze.json").read_text())
    for name,pin in frozen["files"].items():
        data=(HERE/name).read_bytes()
        require(__import__("hashlib").sha256(data).hexdigest()==pin,"frozen public input "+name)
    declaration = loads((HERE / "declared-inputs.json").read_text())
    pins = loads((HERE / "source-pins.json").read_text())
    helper_pins = loads((HERE / "helper-source-pins.json").read_text())
    with zipfile.ZipFile(jar) as archive:
        require(all(C.digest(jar) == JAR for _ in (0,)), "JAR continuity")
        for name, item in {**pins["classes"], **helper_pins["classes"]}.items():
            require(I.inspect_class(archive.read(name + ".class"))["class_sha256"] == item["sha256"], "actual class origin/pin: " + name)
    authored = [HERE / name for name in frozen["files"]] + [HERE / "input-freeze.json"]
    dependencies = [ROOT / "tests/reference/client/controller/capture.py",
                    ROOT / "tests/reference/client/controller/classfile.py",
                    ROOT / "tests/reference/client/picking/observe.py",
                    ROOT / "tests/tools/client/test_picking.py",
                    ROOT / "tests/tools/world/test_source_saving.py"]
    require({p.relative_to(ROOT).as_posix():C.digest(p) for p in dependencies}==frozen["support_sha256"],
            "frozen actual capture/class/input producer dependencies")
    inputs = [jar, *libraries, java, javac, jvm.resolve(strict=True), jmod.resolve(strict=True),
              jdk / "lib/modules", unsupported.resolve(strict=True), *java.parent.glob("*.dll"), *authored, *dependencies,
              *(HERE / name for name in frozen["files"])]
    before = {str(p.resolve()): C.digest(p) for p in sorted(set(inputs))}
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    classes = output / "classes"; classes.mkdir()
    environment = os.environ.copy()
    removed = [name for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS") if environment.pop(name, None) is not None]
    classpath = os.pathsep.join(map(str, [classes, jar, *libraries]))
    flags = ["-Xint", "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
             "--add-opens", "java.base/java.util=ALL-UNNAMED", "--add-opens", "java.base/java.lang=ALL-UNNAMED"]
    compile_command = [str(javac), "-encoding", "UTF-8", "-implicit:none", "-cp", classpath,
                       "-d", str(classes), str(HERE / "RedstoneTorchObservation.java")]
    requested_jdk = loads((HERE / "jdk-methods.json").read_text())
    with zipfile.ZipFile(jmod) as module_archive:
        jdk_code = {name:I.inspect_class(module_archive.read("classes/" + name + ".class"))
                    for name in requested_jdk["classes"]}
    with zipfile.ZipFile(unsupported) as module_archive:
        jdk_code["sun/misc/Unsafe"] = I.inspect_class(module_archive.read("classes/sun/misc/Unsafe.class"))
    for metadata in ("metadata-probe-pins.json", "jdk-accounting-pins.json"):
        metadata_pins = loads((HERE / metadata).read_text())
        require(all(jdk_code[name]["class_sha256"] == value["class_sha256"]
                    for name, value in metadata_pins["classes"].items()), "evidenced passive probe and holder classes")
    write(output / "jdk-code-pins.json", dict(jmod_sha256=C.digest(jmod),
        unsupported_sha256=C.digest(unsupported), classes=jdk_code))
    runs = {}
    for cohort in declaration["cohorts"]:
        roles = list(declaration["world_roles"].values()) if cohort == "aging" else [declaration["world_roles"]["0"]]
        for repeat in range(2):
            run = output / (cohort + "-ordinary-" + str(repeat)); run.mkdir()
            worlds = run / "worlds"; worlds.mkdir()
            for role in roles:
                shutil.copytree(HERE / "world-inputs" / role["name"], worlds / role["name"])
            command = [str(java), *flags, "-cp", classpath, "RedstoneTorchObservation", str(jar), str(worlds),
                       str(HERE / (cohort + "-profiles.tsv")), cohort]
            runs[(cohort, repeat)] = (run, worlds, inventory(worlds), command, roles)
    write(output / "freeze.json", dict(inputs_before=before, input_freeze_sha256=C.digest(HERE / "input-freeze.json"),
        jdk_code_pins_sha256=C.digest(output / "jdk-code-pins.json"),
        compile_argv=compile_command, ordinary_argv={c+"-"+str(n):r[3] for (c,n),r in runs.items()},
        mutable_worlds_before={c+"-"+str(n):r[2] for (c,n),r in runs.items()}, removed_environment=removed,
        budget=dict(compile=30, ordinary=90, stdout=C.STDOUT_LIMIT, stderr=C.STDERR_LIMIT), ordinary_flags=flags))
    report = dict(status="FAIL", runs=[], inputs_before=before, no_game_class_replacements=True,
        prepared_root=str(HERE),repository_root=str(ROOT),capture_root=str(output),
        prepared_relative="tests/reference/world/redstone_torch",
        classpath_separator=os.pathsep,helper_directory_url=classes.as_uri().replace("file:///","file:/")+"/",
        paths=dict(java=str(java),javac=str(javac),jvm=str(jvm.resolve()),jmod=str(jmod),
                   unsupported=str(unsupported),modules=str(jdk/"lib/modules"),
                   libraries=list(map(str,libraries)),java_dlls=list(map(str,java.parent.glob("*.dll")))),
        public_fresh_capture=True,full_entropy_qualification=False)
    primary = None
    try:
        for role, tool in (("java", java), ("javac", javac)):
            result = C.capture([str(tool), "-version"], output, role, 10, environment)
            report[role] = result; C.require_success(result)
            version = (output / (role+".stdout")).read_text() + (output / (role+".stderr")).read_text()
            require("17.0.17" in version, "qualified existing JDK17.0.17")
        compiled = C.capture(compile_command, output, "compile", 30, environment)
        report["compile"] = compiled; C.require_success(compiled, empty_stderr=True)
        report["helpers_before"] = inventory(classes)
        require(set(report["helpers_before"]) == {"RedstoneTorchObservation.class",
            "RedstoneTorchObservation$1.class", "RedstoneTorchObservation$Observer.class"}, "only three authored helper classes")
        # Capture every fixed opportunity before interpreting any Source run.
        for (cohort, repeat), (run, worlds, prior, command, roles) in runs.items():
            item = dict(cohort=cohort, repeat=repeat, world_before=prior, command=command)
            report["runs"].append(item)
            try: item["capture"] = C.capture(command, run, "original", 90, environment, run)
            except BaseException as error: item["launch_failure"] = dict(type=type(error).__name__, message=str(error))
            finally:
                item["world_after"] = inventory(worlds)
                write(output / "capture.json", report)
        projections = {}
        for item in report["runs"]:
            cohort, repeat = item["cohort"], item["repeat"]
            run, worlds, prior, command, roles = runs[(cohort, repeat)]
            require("launch_failure" not in item, "retained process launch failure")
            C.require_success(item["capture"])
            raw = (run / "original.stdout").read_bytes()
            rows, own, failures = decode(raw, cohort, declaration["cohorts"][cohort], roles)
            selected = "".join("DB\t" + json.dumps(row, separators=(",", ":")) + "\n" for row in rows).encode("ascii")
            (run / "selected.txt").write_bytes(selected)
            for name, values in (("own-entropy.jsonl", own), ("source-exceptions.jsonl", failures)):
                (run / name).write_text("".join(json.dumps(value, separators=(",", ":")) + "\n" for value in values), newline="\n")
            for row in rows:T.validate(row,roles)
            item["own_entropy"] = A.audit(rows, own)
            item["rows"] = len(rows); item["selected_sha256"] = C.digest(run / "selected.txt")
            require({k:v for k,v in prior.items() if not k.endswith("/session.lock")} ==
                    {k:v for k,v in item["world_after"].items() if not k.endswith("/session.lock")},
                    "saved input bytes except ordinary session locks")
            stderr = (run / "original.stderr").read_text()
            location = classes.as_uri().replace("file:///", "file:/") + "/"
            require(stderr.splitlines() == [
                "WARNING: A terminally deprecated method in java.lang.System has been called",
                "WARNING: System::setSecurityManager has been called by RedstoneTorchObservation (" + location + ")",
                "WARNING: Please consider reporting this to the maintainers of RedstoneTorchObservation",
                "WARNING: System::setSecurityManager will be removed in a future release"],
                "closed existing JDK17 deprecation diagnostic only; retain any mismatch")
            projections[(cohort, repeat)] = rows
        require(all(same(projections[(c,0)], projections[(c,1)]) for c in declaration["cohorts"]),
                "two prescribed ordinary VM logical cohorts identical")
        report["status"] = "PASS_WITH_UNOBSERVED_ENTITY_FRONTIER"
    except BaseException as error:
        primary = error; report["failure"] = dict(type=type(error).__name__, message=str(error))
    finally:
        report["inputs_after"] = {p:C.digest(Path(p)) for p in before}
        report["helpers_after"] = inventory(classes)
        report["inputs_unchanged"] = before == report["inputs_after"]
        report["helpers_unchanged"] = report.get("helpers_before") == report["helpers_after"]
        if not report["inputs_unchanged"] or not report["helpers_unchanged"]:
            report["status"] = "FAIL"
            if primary is None: primary = AssertionError("frozen inputs/helper continuity")
        write(output / "capture.json", report)
    if primary is not None: raise primary
    print("PASS_WITH_UNOBSERVED_ENTITY_FRONTIER actual DB203 x2 ordinary cohorts;", output / "capture.json")


if __name__ == "__main__":
    main()
