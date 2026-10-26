"""Fresh finite genuine kq/xi/vu/gr/fd/dc statistic dispatch comparison."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from urllib.parse import unquote, urlparse
import zipfile
sys.dont_write_bytecode = True
from support import require, rows, run

HERE = Path(__file__).resolve().parent
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5"}
NATIVES = {"lwjgl64.dll": "86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7"}


def fingerprint(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def success(result):
    require(result.returncode == 0 and result.stderr == b"", "process failed: " + repr(result))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client-jar", "libraries", "natives", "java", "javac", "native-probe", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--native-build-manifest", type=Path)
    args = parser.parse_args()
    if os.name != "nt":
        print("Genuine kq profile currently qualified on Windows x64 only", file=sys.stderr)
        return 77
    require(args.java.resolve().parent == args.javac.resolve().parent, "explicit matching JDK17 tools")
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        require(not os.environ.get(key), "ambient Java option rejected: " + key)
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "CLASSPATH"):
        env.pop(key, None)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="player-statistic-reference-", dir=args.work_dir.resolve()))
    report = {"status": "RUNNING", "work_directory": str(work)}
    try:
        pins = json.loads((HERE / "source-pins.json").read_text(encoding="utf-8"))
        paths = [args.client_jar, args.java, args.javac, args.native_probe] + [args.libraries / x for x in LIBRARIES] + [args.natives / x for x in NATIVES]
        inputs = {str(path.resolve(strict=True)): fingerprint(path) for path in paths}
        report["inputs"] = inputs
        require(fingerprint(args.client_jar) == pins["client_jar"], "whole original JAR pin")
        for name, wanted in {**LIBRARIES, **NATIVES}.items():
            path = (args.libraries if name.endswith(".jar") else args.natives) / name
            require(fingerprint(path)["sha256"] == wanted, "SDK pin " + name)
        sources = {str(path.relative_to(HERE)): fingerprint(path) for path in HERE.iterdir() if path.is_file()}
        report["sources"] = sources
        provenance = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
        for name, wanted in provenance["reference_files"].items():
            require(fingerprint(HERE / name) == wanted, "reference source pin " + name)
        repo = HERE.parents[3]
        native_sources = {name: fingerprint(repo / name) for name in provenance["native_sources"]}
        require(native_sources == provenance["native_sources"], "native source cohort pins")
        report["native_sources"] = native_sources
        parser_path = HERE.parent / "notice/classfile.py"
        spec = importlib.util.spec_from_file_location("mc_statistic_classfile", parser_path)
        require(spec is not None and spec.loader is not None, "classfile parser")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        resources = work / "resources"
        with zipfile.ZipFile(args.client_jar) as archive:
            for name, wanted in pins["classes"].items():
                require(module.inspect_class(archive.read(name.replace(".", "/") + ".class")) == wanted,
                    "original class/Code pin " + name)
            from support import verify_water_instruction_sites
            report["water_instruction_sites"] = verify_water_instruction_sites(archive.read("vs.class"))
            for name, wanted in pins["resources"].items():
                require(name in ("lang/en_US.lang", "lang/stats_US.lang", "achievement/map.txt"), "resource allowlist")
                data = archive.read(name)
                require({"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} == wanted, "resource pin")
                path = resources / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        report["tools"] = {}
        for name, path in (("java", args.java), ("javac", args.javac)):
            result = run([path.resolve(), "-version"], work, name + "-version", env=env)
            require(result.returncode == 0 and re.search(rb'(?:version "|javac )17\.', result.stdout + result.stderr), "JDK17 version")
            report["tools"][name] = {"capture": result.capture_report, "version": (result.stdout + result.stderr).decode("utf-8")}
        classes = work / "helpers"
        classes.mkdir()
        classpath = os.pathsep.join(str(path.resolve()) for path in [classes, args.client_jar] + [args.libraries / name for name in LIBRARIES])
        compiled = run([args.javac.resolve(), "--release", "17", "-encoding", "UTF-8", "-classpath", classpath,
            "-d", classes, HERE / "PlayerStatisticsObservation.java"], work, "compile", env=env)
        success(compiled)
        helpers = {str(path.relative_to(classes)): fingerprint(path) for path in classes.rglob("*.class")}
        require(set(helpers) == {"PlayerStatisticsObservation.class"}, "helper-only classpath")
        report["helpers"] = helpers
        names = work / "classes.txt"
        names.write_text("\n".join(pins["classes"]) + "\n", encoding="ascii", newline="\n")
        binding = work / "binding.tsv"
        original = run([args.java.resolve(), "-Xint", "-Xverify:all", "-Xmx512m",
            "-Xlog:class+init=info:file=class-init.log", "-Dorg.lwjgl.librarypath=" + str(args.natives.resolve()),
            "-classpath", classpath, "PlayerStatisticsObservation", work / "original-game", names, binding], work, "original", env=env)
        success(original)
        bound = {}
        for line in binding.read_text(encoding="utf-8").splitlines():
            name, extent, digest, origin = line.split("\t")
            require(name not in bound and name in pins["classes"], "binding class inventory")
            uri = urlparse(origin)
            path = unquote(uri.path)
            if re.match(r"^/[A-Za-z]:", path):
                path = path[1:]
            pin = pins["classes"][name]
            require(uri.scheme == "file" and Path(path).resolve() == args.client_jar.resolve(), "runtime original origin " + name)
            require(int(extent) == pin["class_bytes"] and digest == pin["class_sha256"], "runtime class binding " + name)
            bound[name] = {"bytes": int(extent), "sha256": digest, "origin": origin}
        require(set(bound) == set(pins["classes"]), "runtime binding completeness")
        report["runtime_binding"] = bound
        initialized = (work / "class-init.log").read_text(encoding="utf-8")
        for forbidden in ("org/lwjgl/openal/AL", "org/lwjgl/opengl/Display"):
            require("Initializing '" + forbidden + "'" not in initialized, "unexpected device class initialization " + forbidden)
        native_game = work / "native-game"
        native_game.mkdir()
        native = run([args.native_probe.resolve(), resources, native_game], work, "native", env=env)
        success(native)
        original_rows, _ = rows(original.stdout, native=False)
        native_rows, checks = rows(native.stdout, native=True)
        require(original_rows == native_rows, "actual statistic projections differ")
        require(inputs == {str(path.resolve()): fingerprint(path) for path in paths}, "binary inputs changed")
        require(sources == {str(path.relative_to(HERE)): fingerprint(path) for path in HERE.iterdir() if path.is_file()}, "reference sources changed")
        require(helpers == {str(path.relative_to(classes)): fingerprint(path) for path in classes.rglob("*.class")}, "helper bytes changed")
        require(native_sources == {name: fingerprint(repo / name) for name in native_sources}, "native sources changed")
        if args.native_build_manifest:
            report["native_build_manifest"] = {"fingerprint": fingerprint(args.native_build_manifest),
                "record": json.loads(args.native_build_manifest.read_text(encoding="utf-8"))}
        report.update(status="PASS", rows=len(native_rows), native_checks=checks,
            original_capture=original.capture_report, native_capture=native.capture_report,
            original_banners=[line.decode("ascii") for line in original.stdout.splitlines() if line in (b"151 recipes", b"16 achievements")],
            native_banners=[line.decode("ascii") for line in native.stdout.splitlines() if line in (b"151 recipes", b"16 achievements")],
            effect_projection="Actual Minecraft.ad vs/aw constructor fields, Block8 texture205, five independent arrays, all raw zero bits compare exactly; no animation/GL body is executed.",
            constructor_scope="Selected u78 -> genuine ad119/122 -> jl186 -> Session -> xi/I/kh owner portions. Full startup and texture registration at510 remain incomplete.",
            class_init=fingerprint(work / "class-init.log"))
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({key: report[key] for key in ("status", "work_directory", "rows", "native_checks", "error") if key in report}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
