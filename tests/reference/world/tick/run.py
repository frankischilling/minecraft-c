"""Owning scheduler contracts and bounded unchanged-client replay."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
LIBRARIES = {
    "lwjgl-2.9.0.jar": "c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a",
    "lwjgl_util-2.9.0.jar": "474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad",
    "jinput-2.0.5.jar": "79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018",
    "jutils-1.0.0.jar": "c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5",
}


def need(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def projection(text):
    rows = [line for line in text.splitlines() if line.startswith(("TICK ", "DRAIN ", "SPAWN "))]
    need(len(rows) == 11 and len({tuple(row.split()[:2]) for row in rows}) == 11,
         "scheduler observation inventory")
    return sorted(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--client-jar", type=Path)
    parser.add_argument("--library-dir", type=Path)
    parser.add_argument("--java", type=Path)
    args = parser.parse_args()
    support_path = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("world_tick_support", support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="tick-", dir=args.work_dir.resolve()))
    report = {"status": "FAIL", "scope": "11 unchanged-client scheduler prefixes; native authored-Chunk CQ false/true loops, publication/group/color/jockey contracts; eight complete 361-Chunk FD.n continuations with independent Random/LCG/Grass-draw oracles and retained null-Random failure prefix"}
    inputs = [args.native.resolve(), Path(__file__).resolve(), support_path,
              HERE / "observations.json"]
    env = os.environ.copy()
    for name in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        env.pop(name, None)
    before = None
    classes = None
    helper_before = None

    def run(command, prefix, timeout=100, empty_stderr=True):
        result = support.run_bounded(command, cwd=output, prefix=prefix, timeout=timeout, env=env)
        need(result.returncode == 0, f"{prefix} exit {result.returncode}")
        need(not empty_stderr or not result.stderr, f"{prefix} nonempty stderr")
        return result

    try:
        expected = sorted(json.loads((HERE / "observations.json").read_text())["rows"])
        need(len(expected) == 11, "captured observation inventory")
        if args.client_jar is not None:
            need(args.library_dir is not None, "--library-dir required with original replay")
            jar = args.client_jar.resolve()
            need(digest(jar) == JAR, "original client JAR differs")
            libraries = []
            for name, pin in LIBRARIES.items():
                matches = list(args.library_dir.resolve().rglob(name))
                need(len(matches) == 1, "reference library inventory: " + name)
                path = matches[0]
                need(digest(path) == pin, "reference library differs: " + name)
                libraries.append(path)
            java = (args.java or Path(shutil.which("java") or "")).resolve(strict=True)
            javac = java.with_name("javac.exe" if os.name == "nt" else "javac")
            need(javac.is_file() and java.parent.samefile(javac.parent), "matching same-bin JDK")
            pins = json.loads((HERE / "source-pins.json").read_text())
            with zipfile.ZipFile(jar) as archive:
                for name, pin in pins["classes"].items():
                    need(hashlib.sha256(archive.read(name + ".class")).hexdigest() == pin,
                         "original whole class differs: " + name)
            inputs += [jar, java, javac, *libraries, HERE / "source-pins.json",
                       HERE / "SchedulerObservation.java"]
        before = {str(path): digest(path) for path in inputs}
        report["input_before_sha256"] = before
        if args.client_jar is not None:
            vm = run([java, "-version"], "java-version", 10, False)
            compiler = run([javac, "-version"], "javac-version", 10, False)
            version = re.search(r'version "(17\.[0-9]+\.[0-9]+)',
                                (vm.stdout + vm.stderr).decode("utf-8", "strict"))
            compiled = re.search(r"javac (17\.[0-9]+\.[0-9]+)",
                                 (compiler.stdout + compiler.stderr).decode("utf-8", "strict"))
            need(version and compiled and version[1] == compiled[1], "matching JDK17 required")
            classes = output / "classes"
            classes.mkdir()
            cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
            run([javac, "-encoding", "UTF-8", "-classpath", cp, "-d", classes,
                 HERE / "SchedulerObservation.java"], "compile", 30)
            helper_before = {path.name: digest(path) for path in classes.glob("*.class")}
            need(set(helper_before) == {"SchedulerObservation.class",
                "SchedulerObservation$StopBiome.class", "SchedulerObservation$ProbeManager.class",
                "SchedulerObservation$FlatWorld.class", "SchedulerObservation$RecordingRandom.class",
                "SchedulerObservation$StopSpawn.class"},
                "unexpected helper inventory")
            report["helper_before_sha256"] = helper_before
            original = run([java, "--add-opens", "java.base/java.util=ALL-UNNAMED",
                "-Xint", "-Xverify:all", "-classpath", cp,
                "SchedulerObservation", output / "original-worlds", jar], "original")
            left = original.stdout.decode("ascii", "strict")
            origins = [line.split(" ", 2) for line in left.splitlines() if line.startswith("ORIGIN ")]
            need([line[1] for line in origins] == list(pins["classes"]), "original class inventory")
            need(all(Path(line[2]).resolve().samefile(jar) for line in origins), "foreign game class")
            need(left.splitlines()[-1] == "Original World scheduler: 3 tick states, 6 drain states, 2 spawn prefixes",
                 "original completion marker")
            need(projection(left) == expected, "original observations changed")
            report["original"] = original.capture_report
        native = run([args.native.resolve(), "world-tick",
                      str(output / "native-worlds").encode("utf-8").hex()], "native")
        text = native.stdout.decode("utf-8", "strict")
        need(text.splitlines() and re.fullmatch(r"World scheduler: [1-9][0-9]* checks", text.splitlines()[-1]),
             "native completion marker")
        need(projection(text) == expected, "native observations differ from original capture")
        mob_rows = [line for line in text.splitlines() if line.startswith("MOB_LOOP ")]
        need(len(mob_rows) == 9, "native completed spawning contract inventory")
        for index, name in ((0, "wh"), (6, "uz"), (7, "dl")):
            row = re.fullmatch(
                rf"MOB_LOOP publish {name} completed=1 group=4 membership=4 total=([0-9]+) random-checked=1",
                mob_rows[index])
            need(row is not None and int(row[1]) >= 6,
                 "real group/publication/cumulative/World Random contract: " + name)
        for index, name in enumerate(("cn", "uz", "fr", "gb", "uw"), 1):
            row = re.fullmatch(
                rf"MOB_LOOP reject {name} completed=1 constructors=([0-9]+) yaw-checked=1 random-checked=1",
                mob_rows[index])
            need(row is not None and int(row[1]) > 1,
                 "real rejected-predicate continuation contract: " + name)
        need(mob_rows[8] == "MOB_LOOP jockey cn next=cq845:56 published=2 skeleton=fr random-checked=1",
             "actual Skeleton publication before missing mount body")
        random_rows = [line for line in text.splitlines()
                       if line.startswith(("RANDOM_TICK publish ", "RANDOM_TICK reject "))]
        expected_random = [("publish", "wh"), *(('reject', name)
            for name in ("cn", "uz", "fr", "gb", "uw")), ("publish", "uz"), ("publish", "dl")]
        need(len(random_rows) == len(expected_random), "native complete FD.n contract inventory")
        for line, (kind, name) in zip(random_rows, expected_random):
            row = re.fullmatch(
                rf"RANDOM_TICK {kind} {name} chunks=361 samples=28880 lcg=([0-9]+) "
                r"snow=([0-9]+) ambient=([0-9]+) world-draws=([0-9]+) block-draws=([0-9]+) "
                r"block-ticks=([0-9]+) source=198:994 nested-holds=2", line)
            need(row is not None, "complete source-return/counter/hold contract: " + kind + name)
            lcg, snow, ambient, world_draws, block_draws, block_ticks = map(int, row.groups())
            need(0 <= snow <= 361 and ambient in (0, 361) and lcg == 28880 + snow + ambient,
                 "independent weather/sample LCG accounting: " + kind + name)
            need(world_draws in (722, 723) and (world_draws == 722 or ambient == 361),
                 "mandatory per-Chunk and reached ambient Random accounting: " + kind + name)
            need(block_draws == block_ticks * (0 if kind == "publish" and name == "uz" else 3),
                 "reached actual Grass Random accounting: " + kind + name)
        prefix_rows = [line for line in text.splitlines() if line.startswith("RANDOM_TICK prefix ")]
        need(prefix_rows == ["RANDOM_TICK prefix fd195:470 chunk-held=1 world-holds=2 math-holds=2 source-preserved=1"],
             "retained typed null-Random prefix and exact cleanup without class/source reset")
        report.update(status="PASS", native=native.capture_report, observations=11,
                      native_completed_spawn_contracts=9, native_completed_random_contracts=8,
                      native_retained_random_prefixes=1, original_replay=args.client_jar is not None)
    except BaseException as exc:
        report["error"] = {"type": type(exc).__name__, "message": str(exc)}
        raise
    finally:
        after = {str(path): digest(path) for path in inputs}
        report["input_after_sha256"] = after
        if before is not None and before != after:
            report.update(status="FAIL", input_changed=True)
        if classes is not None:
            helper_after = {path.name: digest(path) for path in classes.glob("*.class")}
            report["helper_after_sha256"] = helper_after
            if helper_before != helper_after:
                report.update(status="FAIL", helper_changed=True)
        report["products"] = {path.name: digest(path) for path in output.iterdir() if path.is_file()}
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    need(report["status"] == "PASS", "scheduler replay input integrity")
    print(f"World scheduler: 11 observations; report {output / 'report.json'}")


if __name__ == "__main__":
    main()
