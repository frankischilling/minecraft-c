"""Optional JDK17 target-retirement control; no game JAR or native executable."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import tempfile

import run as driver


BLOCKING_HELPER = """public final class ParticleObservation {
    public static void main(String[] ignored) throws Exception { Thread.sleep(60000); }
}
"""

PROCESS_CHECK = """import java.time.Instant;
import java.util.concurrent.TimeUnit;
public final class ParticleProcessCheck {
    public static void main(String[] a) throws Exception {
        if(a.length!=4)throw new IllegalArgumentException("mode pid start marker");
        long pid=Long.parseLong(a[1]);
        ProcessHandle target=ProcessHandle.of(pid).orElse(null);
        if(target==null||!target.isAlive()){System.out.println("target\\tGONE");return;}
        ProcessHandle.Info info=target.info();
        if(!info.startInstant().orElseThrow().equals(Instant.parse(a[2]))||
           !info.commandLine().orElseThrow().contains(a[3])||
           !info.commandLine().orElseThrow().contains("ParticleObservation"))
            throw new AssertionError("target identity mismatch; refusing termination");
        if(a[0].equals("check")){System.out.println("target\\tOWNED_ALIVE");System.exit(1);}
        if(!a[0].equals("kill"))throw new IllegalArgumentException("mode");
        target.destroyForcibly();target.onExit().get(10,TimeUnit.SECONDS);
        if(target.isAlive())throw new AssertionError("owned target not reaped");
        System.out.println("target\\tKILLED");
    }
}
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    java = args.java.resolve(strict=True)
    javac = java.with_name("javac.exe" if os.name == "nt" else "javac")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="particle-cleanup-", dir=args.work_dir.resolve()))
    collector = Path(__file__).with_name("ParticleInputs.java").resolve(strict=True)
    before = {str(p): driver.sha(p) for p in (java, javac, collector, Path(__file__).resolve(),
        Path(driver.__file__).resolve(), driver.REFERENCE / "statistics/support.py")}
    classes = root / "classes"
    classes.mkdir()
    target = root / "ParticleObservation.java"
    target.write_text(BLOCKING_HELPER, encoding="utf-8", newline="\n")
    checker = root / "ParticleProcessCheck.java"
    checker.write_text(PROCESS_CHECK, encoding="utf-8", newline="\n")
    for label, tool in (("java", java), ("javac", javac)):
        result = driver.run_bounded([str(tool), "-version"], root, label, timeout=10,
            env=driver.controlled_environment())
        driver.need(result.returncode == 0 and re.search(rb'(?:version\s+"|javac\s+)17\.',
            result.stdout + result.stderr), "explicit JDK17 " + label)
    driver.run([str(javac), "--add-modules", "jdk.jdi", "-encoding", "UTF-8", "-proc:none",
                "-cp", str(classes), "-d", str(classes),
                str(collector), str(target), str(checker)], root, "compile")
    inventory = driver.helper_inventory(classes)
    driver.need(set(inventory) == {"ParticleInputs.class", "ParticleInputs$Drain.class",
        "ParticleObservation.class", "ParticleProcessCheck.class"}, "authored control helper inventory")
    metadata = root / "target-process.tsv"
    # Windows reaches Path.of failure before any reader starts. POSIX has no
    # invalid non-NUL argv path character, so it exercises reader-open failure.
    bad_output = "?" if os.name == "nt" else str(root / "existing-directory")
    if os.name != "nt":
        Path(bad_output).mkdir()
    expected = b"java.nio.file.InvalidPathException:" if os.name == "nt" else b"java.lang.AssertionError: observer bound/reader failure"
    checked = None
    report = {"status": "FAIL", "scope": "authored blocking helper lifecycle; no original game comparison",
        "inputs_before": before, "helpers_before": inventory}
    try:
        result = driver.run_bounded([str(java), "--add-modules", "jdk.jdi", "-cp", str(classes),
            "ParticleInputs", str(classes), bad_output, str(root / "target.stderr"),
            str(root / "unused.jar"), str(root / "unused-origins.tsv"), str(metadata)],
            root, "collector", timeout=25, env=driver.controlled_environment())
        report["collector"] = result.capture_report
        fields = metadata.read_text(encoding="utf-8").splitlines()
        driver.need(len(fields) == 1, "one actual target metadata row")
        values = fields[0].split("\t")
        driver.need(len(values) == 3 and all(re.fullmatch(r"[1-9][0-9]*", x) for x in values[:2]),
                    "actual target/collector IDs")
        report["target_process"] = values
        checked = driver.run_bounded([str(java), "-cp", str(classes), "ParticleProcessCheck", "check",
            values[0], values[2], str(classes)], root, "target-check", timeout=15,
            env=driver.controlled_environment())
        report["target_check"] = checked.capture_report
        driver.need(checked.returncode == 0 and checked.stdout.splitlines() == [b"target\tGONE"]
                    and checked.stderr == b"", "owned target must be gone")
        lines = result.stderr.splitlines()
        driver.need(result.returncode == 1 and result.stdout == b"" and bool(lines) and
            lines[0].startswith(b'Exception in thread "main" ' + expected) and
            all(line.startswith(b"\tat ") for line in lines[1:]), "exact expected setup/reader failure")
        report["status"] = "PASS"
    finally:
        # Regression cleanup is permitted only through the JDK identity guard:
        # same actual PID/startInstant and this unique authored target classpath.
        if metadata.exists():
            values = metadata.read_text(encoding="utf-8").strip().split("\t")
            if len(values) == 3:
                cleanup = driver.run_bounded([str(java), "-cp", str(classes), "ParticleProcessCheck", "kill",
                    values[0], values[2], str(classes)], root, "regression-retirement", timeout=15,
                    env=driver.controlled_environment())
                report["regression_retirement"] = cleanup.capture_report
                driver.need(cleanup.returncode == 0 and cleanup.stderr == b"" and
                    cleanup.stdout.splitlines() in ([b"target\tGONE"], [b"target\tKILLED"]), "guarded target retirement")
        report["inputs_after"] = {p: driver.sha(Path(p)) for p in before}
        report["helpers_after"] = driver.helper_inventory(classes)
        if report["inputs_after"] != before or report["helpers_after"] != inventory:
            report["status"] = "FAIL"
        (root / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        driver.need(report["inputs_after"] == before and report["helpers_after"] == inventory,
                    "unchanged collector/tools/helper products")
    print("Particle target cleanup: PASS; report " + str(root / "report.json"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
