"""Run fresh unchanged-original and production-native Worlds, then compare exactly."""
import argparse
import concurrent.futures
import datetime
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
from binary_pins import verify_client, verify_paired_server
from processes import bounded, successful
from records import compare, require, sha256

HERE = Path(__file__).resolve().parent


def tools(java, javac, env, root):
    result = {}
    for name, path in (("java", java), ("javac", javac)):
        directory = root / (name + "-version")
        directory.mkdir()
        process, _ = bounded([str(path), "-version"], 30, env, 65536, directory)
        (directory / "process.json").write_bytes((json.dumps(process, indent=2) + "\n").encode())
        require(successful(process), f"failed bounded {name} version probe: {process}")
        version = ((directory / "stdout.log").read_bytes() +
                   (directory / "stderr.log").read_bytes()).decode("utf-8", errors="strict")
        require(re.search(r"(?:version\s+\"?|javac\s+)17(?:\.|\")", version) is not None,
                f"{name} must be an explicit JDK17 tool, got: {version.strip()}")
        result[name] = {"path": str(path), "sha256": sha256(path), "version": version.strip(),
                        "version_probe": process}
    return result


def resources(root):
    language = root / "lang"
    language.mkdir(parents=True)
    (root / "achievement").mkdir()
    (root / "achievement/map.txt").write_bytes(b"")
    (language / "en_US.lang").write_bytes(b"tile.stone.name=Authored stone\n")
    (language / "stats_US.lang").write_bytes(
        b"stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
        b"stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n")


def execute(command, directory, timeout, env):
    result, _ = bounded(command, timeout, env, 16 * 1024 * 1024, directory)
    result["stderr_bytes"] = (directory / "stderr.log").stat().st_size
    (directory / "process.json").write_bytes((json.dumps(result, indent=2) + "\n").encode())
    require(successful(result), f"bounded process failed; inspect {directory}")
    require(result["stderr_bytes"] == 0, f"unexpected process stderr; inspect {directory}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-jar", type=Path, required=True)
    parser.add_argument("--server-jar", type=Path, help="optional paired static metadata verification only")
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--javac", type=Path, required=True)
    parser.add_argument("--native-probe", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--profiles", nargs="+", choices=("view", "fresh", "warm"), default=["view"])
    parser.add_argument("--timeout", type=int, default=1800, help="maximum seconds per process")
    parser.add_argument("--jobs", type=int, choices=(1, 2), default=2)
    parser.add_argument("--tested-source-id", default="", help="caller-provided configured source identity")
    parser.add_argument("--native-build-manifest", type=Path, help="optional caller build/source/archive provenance")
    args = parser.parse_args()
    for name in ("client_jar", "java", "javac", "native_probe"):
        path = getattr(args, name).resolve()
        if not path.is_file():
            print(f"Missing explicitly supplied {name}: {path}", file=sys.stderr)
            return 77
        setattr(args, name, path)
    require(args.timeout > 0, "timeout must be positive")
    env = os.environ.copy()
    for key in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(key, None)
    args.work_dir = args.work_dir.resolve()
    require(args.work_dir != HERE and HERE not in args.work_dir.parents,
            "generated captures must be outside the source package")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
    run = Path(tempfile.mkdtemp(prefix=stamp, dir=args.work_dir))
    classes = run / "classes"
    classes.mkdir()
    report = {"schema": 1, "status": "RUNNING", "run": str(run),
              "profiles": {}, "tested_source_id": args.tested_source_id,
              "native_program": {"path": str(args.native_probe), "sha256": sha256(args.native_probe)},
              "authored_sources": {p.name: sha256(p) for p in HERE.iterdir()
                                   if p.is_file() and p.suffix in (".c", ".h", ".java", ".py")},
              "scope": "Controlled post-constructor World.r input only; no uncontrolled startup transient-parity claim."}
    if args.native_build_manifest:
        report["caller_native_build_manifest"] = {
            "path": str(args.native_build_manifest.resolve()),
            "sha256": sha256(args.native_build_manifest),
            "record": json.loads(args.native_build_manifest.read_text(encoding="utf-8-sig"))}
    try:
        binary = verify_client(args.client_jar)
        report["client_binary"] = binary
        report["tools"] = tools(args.java, args.javac, env, run)
        report["paired_static_metadata_verification"] = (
            verify_paired_server(args.server_jar.resolve()) if args.server_jar else None)
        compile_command = [str(args.javac), "--release", "17", "-classpath", str(args.client_jar),
                           "-d", str(classes), str(HERE / "WorldPropagationProbe.java")]
        compile_dir = run / "compile"
        compile_dir.mkdir()
        report["compile"] = execute(compile_command, compile_dir, 60, env)
        report["compiled_observer_classes_before"] = {p.name: sha256(p) for p in classes.glob("*.class")}
        resource_root = run / "resources"
        resources(resource_root)
        processes = {}

        def launch(side, profile):
            directory = run / side / profile
            directory.mkdir(parents=True)
            if side == "original":
                classpath = os.pathsep.join((str(classes), str(args.client_jar)))
                command = [str(args.java), "-Xverify:all", "-Xmx1g",
                           "--add-opens=java.base/java.util=ALL-UNNAMED", "-classpath", classpath,
                           "WorldPropagationProbe", str(directory / "data"), profile]
            else:
                (directory / "data").mkdir()
                command = [str(args.native_probe), str(resource_root), str(directory / "world"),
                           profile, str(directory / "data")]
            return side, profile, execute(command, directory, args.timeout, env)

        selected = list(dict.fromkeys(args.profiles))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = [pool.submit(launch, side, profile)
                       for profile in selected for side in ("original", "native")]
            for future in concurrent.futures.as_completed(futures):
                side, profile, process = future.result()
                processes.setdefault(profile, {})[side] = process
        report["processes"] = processes
        for profile in selected:
            report["profiles"][profile] = compare(run / "original" / profile,
                                                    run / "native" / profile, profile)
        require(sha256(args.client_jar) == binary["jar_sha256"], "client input changed during execution")
        require(sha256(args.native_probe) == report["native_program"]["sha256"],
                "native executable changed during execution")
        require({p.name: sha256(p) for p in classes.glob("*.class")} ==
                report["compiled_observer_classes_before"], "compiled observer changed during execution")
        require({p.name: sha256(p) for p in HERE.iterdir()
                 if p.is_file() and p.suffix in (".c", ".h", ".java", ".py")} ==
                report["authored_sources"], "authored source changed during execution")
        report["status"] = "PASS"
    except Exception as error:
        report["status"] = "FAIL"
        report["error"] = f"{type(error).__name__}: {error}"
    finally:
        report["compiled_observer_classes"] = {p.name: sha256(p) for p in classes.glob("*.class")}
        report["private_capture_files"] = {str(p.relative_to(run)): {"bytes": p.stat().st_size,
                                            "sha256": sha256(p)} for p in run.rglob("*") if p.is_file()}
        (run / "result.json").write_bytes((json.dumps(report, indent=2) + "\n").encode())
    print(json.dumps({"status": report["status"], "result": str(run / "result.json"),
                      "profiles": {name: {"grid_points": row["grid_points"],
                                           "grid_drain_calls": row["grid_drain_calls"],
                                           "view_drain_calls": row["view_drain_calls"]}
                                   for name, row in report["profiles"].items()},
                      **({"error": report["error"]} if "error" in report else {})}, indent=2))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
