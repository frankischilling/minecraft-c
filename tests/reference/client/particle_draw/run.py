"""Observe genuine original DN/EM draw prefixes in fresh JVMs; no native comparison."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUPPORT = ROOT / "tests/reference/client/controller/capture.py"
INSPECTOR = ROOT / "tests/reference/client/notice/classfile.py"

if hashlib.sha256(SUPPORT.read_bytes()).hexdigest() != "57897f22c1a1f9b6a9c9ee80f96168f782a80e1e5c7578c5d1281768f92f702e":
    raise RuntimeError("changed reviewed bounded capture owner")
if hashlib.sha256(INSPECTOR.read_bytes()).hexdigest() != "a2d6bf0923890ff6bd73c32bc5bcfabf89f883c421a2c1eab4270a3cb060857d":
    raise RuntimeError("changed reviewed offline classfile inspector")


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


CAPTURE = load("effect_render_capture", SUPPORT)
CLASSFILE = load("effect_render_classfile", INSPECTOR)
capture, digest, ensure, require_success = (
    CAPTURE.capture, CAPTURE.digest, CAPTURE.ensure, CAPTURE.require_success)
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}
PARTIALS = ["00000000", "80000000", "3e800000", "3f000000", "3f800000", "bf800000",
            "7f800000", "ff800000", "7fc12345", "7f812345"]
PROFILES = ("empty", "nulltarget", "nullworld")
SOURCE_PROJECTIONS = {
    "empty": {"dn_b_normal_return": 16, "dn_a_null_camera": 1},
    "nulltarget": {"dn_b_particle_invoke": 69, "em_a_null_target": 53},
    "nullworld": {"dn_b_particle_invoke": 69, "em_a_null_world_brightness_invoke": 220},
}


def marker(profile):
    return ("Original DN render: " + profile + "; source-world-clear-complete; network=0; native=0; "
            "no-context; exceptions=10")


def observations(text, profile):
    lines = text.splitlines()
    ensure(1 <= len(lines) <= 160 and all(len(row) <= 32768 for row in lines), "bounded full output")
    rows, exceptions = [], []
    cursor = 0
    while cursor < len(lines):
        row = lines[cursor]
        if row.startswith("DN_EXCEPTION_BEGIN\t"):
            label = row.split("\t")[1]
            end = "DN_EXCEPTION_END\t" + label
            stop = lines.index(end, cursor + 1)
            stack = lines[cursor + 1:stop]
            owner, method = ("dn", "a") if profile == "empty" else ("em", "a")
            ensure(2 <= len(stack) <= 8 and stack[0].startswith("java.lang.NullPointerException") and
                   re.fullmatch(r"\s+at " + owner + r"\." + method + r"\(SourceFile:[0-9]+\)", stack[1]) and
                   all(re.fullmatch(r"\s+at [^\r\n]+", line) for line in stack[2:]), "complete original stack")
            if profile != "empty":
                ensure(len(stack) >= 3 and re.fullmatch(r"\s+at dn\.b\(SourceFile:[0-9]+\)", stack[2]),
                       "actual DN-to-EM call stack")
            exceptions.append({"label": label, "stack": stack})
            cursor = stop + 1
            continue
        ensure(row.startswith(("DN_STATE\t", "DN_DRAW\t", "DN_FAILURE\t", "DN_PARTICLES\t",
                               "DN_NW\t", "DN_INPUT\t")) or row == marker(profile),
               "no unparsed/extraneous output: " + row)
        rows.append(row)
        cursor += 1
    ensure(len(exceptions) == 10 and [item["label"] for item in exceptions] ==
           [("a" if profile == "empty" else "b") + str(i) for i in range(10)], "all10 actual exceptions")
    draw = [row.split("\t") for row in rows if row.startswith("DN_DRAW\t")]
    ensure(draw == [["DN_DRAW", "b", str(i), bits, "return" if profile == "empty" else "throw"]
                    for i, bits in enumerate(PARTIALS)], "all exact raw partial operands/outcomes")
    failures = [row.split("\t") for row in rows if row.startswith("DN_FAILURE\t")]
    ensure([row[1] for row in failures] == [("a" if profile == "empty" else "b") + str(i) for i in range(10)] and
           all(len(row) == 6 and row[2:5] ==
           (["dn", "a", "SourceFile"] if profile == "empty" else ["em", "a", "SourceFile"]) and
           row[5].isdecimal() and int(row[5]) > 0 for row in failures), "actual original source-frame rows")
    states = [row.split("\t") for row in rows if row.startswith("DN_STATE\t")]
    stages = ["constructed"] + ([] if profile == "empty" else ["added"]) + ["after" + str(i) for i in range(10)] + ["cleared"]
    ensure([row[1] for row in states] == stages and all(len(row) == 13 for row in states), "complete DN snapshots")
    for row in states:
        ensure(row[2:7] == ["1"] * 5 and re.fullmatch(r"[0-9a-f]{16},[01],[0-9a-f]{16}", row[7]) and
               re.fullmatch(r"[0-9a-f]{16}", row[8]), "same array/Random, distinctLists, actual null fields and RNG")
        for layer in row[9:]:
            match = re.fullmatch(r"1,([0-9]+),([0-9]+),([0-9]+),([np]+|-)", layer)
            ensure(match and int(match[1]) <= int(match[2]) <= 32 and
                   (match[4] == "-" if int(match[2]) == 0 else len(match[4]) == int(match[2])),
                   "same List, complete live size/capacity/modCount and every backing cell")
    after = [row[2:] for row in states if row[1].startswith("after")]
    ensure(all(row == after[0] for row in after), "all draw invocations preserve exact complete DN state")
    ensure(all(int(layer.split(",")[1]) == 0 for layer in states[-1][9:]), "actual clear empties all four Lists")
    ensure(states[0][7] == states[-1][7], "ordinary DN Random unchanged by adds/draws/World clear")
    particle_rows = [row.split("\t", 2) for row in rows if row.startswith("DN_PARTICLES\t")]
    nw_rows = [row for row in rows if row.startswith("DN_NW\t")]
    input_rows = [row for row in rows if row.startswith("DN_INPUT\t")]
    ensure(len(nw_rows) == len(input_rows) == int(profile != "empty") and
           len(particle_rows) == (0 if profile == "empty" else 11), "separate constructor/NW scope")
    if profile != "empty":
        ensure([row[1] for row in particle_rows] == ["constructed"] + ["after" + str(i) for i in range(10)] and
               all(row[2] == particle_rows[0][2] for row in particle_rows), "all actual JU/EM/Random snapshots stable")
    ensure(rows[-1] == marker(profile) and rows.count(marker(profile)) == 1, "unique normal cleanup receipt last")
    return {"rows": rows[:-1], "exceptions": exceptions,
            "source_pc_projection": SOURCE_PROJECTIONS[profile],
            "pc_projection_authority": "independent original complete-Code mapping, not Java stack BCI"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    parser.add_argument("--jdk", type=Path, required=True)
    parser.add_argument("--profiles", choices=PROFILES, nargs="+", required=True)
    parser.add_argument("--manifest-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    ensure(len(args.profiles) == len(set(args.profiles)), "one fresh JVM per distinct selected profile")
    manifest_path = HERE / "source-pins.json"
    ensure(digest(manifest_path) == args.manifest_sha256, "exact independently reviewed manifest bytes")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    ensure(manifest["client_jar_sha256"] == JAR and manifest["source_projections"] == SOURCE_PROJECTIONS,
           "frozen original authority/static-PC classifications")
    for name in ("EffectRenderObservation.java", "run.py", "README.md"):
        ensure(digest(HERE / name) == manifest["observer_inputs"][name], "frozen reviewed observer input: " + name)
    jdk, jar = args.jdk.resolve(strict=True), args.client_jar.resolve(strict=True)
    suffix = ".exe" if os.name == "nt" else ""
    java, javac = [(jdk / "bin" / (name + suffix)).resolve(strict=True) for name in ("java", "javac")]
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(digest(jar) == JAR and all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
           "unchanged supplied original and exact SDK type archives")
    with zipfile.ZipFile(jar) as archive:
        for name, expected in manifest["classes"].items():
            ensure(CLASSFILE.inspect_class(archive.read(name + ".class")) == expected,
                   "unchanged complete original class and all Code attributes: " + name)
    jdk_meta = {}
    with zipfile.ZipFile(jdk / "jmods/java.base.jmod") as archive:
        for name in manifest["jdk_metadata_class_names"]:
            jdk_meta[name] = CLASSFILE.inspect_class(archive.read("classes/" + name + ".class"))
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve(strict=True)
    classes = output / "classes"
    classes.mkdir()
    inputs = sorted(path for path in jdk.rglob("*") if path.is_file()) + [
        jar, *libraries, HERE / "EffectRenderObservation.java", Path(__file__).resolve(),
        HERE / "README.md", manifest_path, SUPPORT, INSPECTOR,
        ROOT / "tests/reference/client/particles/ParticleObservation.java",
        ROOT / "tests/reference/client/particles/source-pins.json"]
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "selected_profiles": args.profiles, "native_inputs": [], "native_policy": "deny all native links; no context",
              "scope": "Genuine original DN and declared JU/EM CPU constructors/add/draw-failure/World-clear profiles. "
                       "Actual entropy is observed, not supplied; no C peer or native/original equality claim.",
              "jdk_metadata": {"origin": "pinned java.base.jmod entries; lib/modules runtime backing separately hashed",
                               "classes": jdk_meta}}
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH"):
        env.pop(name, None)
    try:
        report["tools"] = []
        versions = []
        for label, tool in (("java", java), ("javac", javac)):
            receipt = capture([str(tool), "-version"], output, label, 10, env)
            report["tools"].append(receipt)
            require_success(receipt)
            versions.append((output / receipt["stdout_file"]).read_text() + (output / receipt["stderr_file"]).read_text())
        runtime, compiler = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0]), re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], "matching JDK17 runtime/compiler")
        cp = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = capture([str(javac), "-encoding", "UTF-8", "-cp", cp, "-d", str(classes),
                           str(HERE / "EffectRenderObservation.java")], output, "compile", 30, env)
        report["compile"] = receipt
        require_success(receipt, empty_stderr=True)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        ensure(set(helper) == {"EffectRenderObservation.class", "EffectRenderObservation$1.class"},
               "only two authored observer classes; no original class replacement")
        report["helper_before_sha256"] = helper
        for profile in args.profiles:
            work = output / profile
            work.mkdir()
            receipt = capture([str(java), "--add-opens", "java.base/java.util=ALL-UNNAMED",
                               "--add-opens", "java.base/java.lang=ALL-UNNAMED", "-Xint", "-Xverify:all",
                               "-Xmx256m", "-Djava.awt.headless=true", "-Djava.security.manager=allow",
                               "-Xlog:class+init=info:file=class-init.log::filesize=2m,filecount=1", "-cp", cp,
                               "EffectRenderObservation", str(jar), profile], output, profile, 60, env, work)
            item = {"name": profile, "capture": receipt, "status": "FAIL"}
            report["profiles"].append(item)
            require_success(receipt)
            text = (output / receipt["stdout_file"]).read_text(encoding="utf-8")
            observed = observations(text, profile)
            stderr = (output / receipt["stderr_file"]).read_text(encoding="utf-8").splitlines()
            ensure(len(stderr) == 4 and stderr[0] == "WARNING: A terminally deprecated method in java.lang.System has been called" and
                   stderr[1] == "WARNING: System::setSecurityManager has been called by EffectRenderObservation (" +
                   classes.as_uri().replace("file:///", "file:/", 1) + "/)" and
                   stderr[2] == "WARNING: Please consider reporting this to the maintainers of EffectRenderObservation" and
                   stderr[3] == "WARNING: System::setSecurityManager will be removed in a future release",
                   "only four fully retained declared JDK guard warnings in stderr")
            log = work / "class-init.log"
            ensure(log.is_file() and log.stat().st_size < 2 * 1024 * 1024 and
                   len(list(work.glob("class-init.log*"))) == 1, "complete bounded initialization log")
            initialized = re.findall(r"Initializing '([^']+)'", log.read_text(encoding="utf-8"))
            forbidden = [name for name in initialized if name.startswith(("org/lwjgl/", "org.lwjgl."))]
            ensure(not forbidden and "dn" in initialized, "no SDK active initialization or native context")
            if profile == "empty":
                ensure(not set(initialized).intersection({"sn", "ju", "em", "xw", "eq", "in", "nw", "ge",
                                                         "java/lang/Math$RandomNumberGeneratorHolder"}),
                       "genuine DN-only early return/null failure, no Entity/Math/NW initialization")
            else:
                ensure(set(["sn", "ju", "em", "xw", "eq", "in", "nw", "ge",
                            "java/lang/Math$RandomNumberGeneratorHolder"]).issubset(initialized) and
                       initialized.index("em") < initialized.index("nw"), "real constructor graph then reached NW CPU init")
            artifact = output / (profile + "-observed.json")
            artifact.write_text(json.dumps(observed, indent=2) + "\n", encoding="utf-8", newline="\n")
            item.update(status="PASS_ORIGINAL_OBSERVATION_ONLY", rows=len(observed["rows"]),
                        observation_sha256=digest(artifact), class_init_log_sha256=digest(log),
                        initialized_original_classes=[name for name in initialized if name in manifest["classes"]])
        report.update(status="PASS_ORIGINAL_OBSERVATION_ONLY", contexts=0, processes=len(args.profiles),
                      claimed_native_parity_rows=0)
    except BaseException as error:
        report["error"] = {"type": type(error).__name__, "message": str(error)}
        raise
    finally:
        report["input_after_sha256"] = {str(path): digest(path) for path in inputs}
        report["helper_after_sha256"] = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob("*.class")}
        same = before == report["input_after_sha256"] and ("helper_before_sha256" not in report or helper == report["helper_after_sha256"])
        if not same:
            report["status"] = "FAIL"
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "all actual JDK/archive/SDK/support/observer/helper input bytes unchanged")
    print("Original DN observation only: " + str(output / "capture.json"))


if __name__ == "__main__":
    main()
