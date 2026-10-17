#!/usr/bin/env python3
"""Compare C vectors with real Java operators and java.util.Random (no game JAR)."""

import argparse
import difflib
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys


def find_tool(override: str | None, name: str) -> Path | None:
    if override:
        found = shutil.which(override)
        return Path(found).resolve() if found else None
    found = shutil.which(name)
    if found:
        return Path(found).resolve()
    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        candidate = Path(java_home) / "bin" / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return candidate.resolve()
    return None


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                          errors="strict", timeout=60, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path, help="C mc_test_java executable")
    parser.add_argument("--java", help="Java runtime executable (otherwise PATH/JAVA_HOME)")
    parser.add_argument("--javac", help="Java compiler executable (otherwise PATH/JAVA_HOME)")
    parser.add_argument("--work-dir", type=Path, default=Path(".local/reference/java-semantics"))
    args = parser.parse_args()
    java = find_tool(args.java, "java")
    javac = find_tool(args.javac, "javac")
    executable = args.executable.resolve()
    missing = []
    if java is None:
        missing.append("java")
    if javac is None:
        missing.append("javac")
    if not executable.is_file():
        missing.append(f"C probe {executable}")
    if missing:
        print("UNAVAILABLE: missing " + ", ".join(missing), file=sys.stderr)
        return 77
    source = Path(__file__).resolve().parents[3] / "tests/reference/java/JavaSemanticsProbe.java"
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    try:
        assert java is not None and javac is not None
        version = run([str(java), "-version"])
        print((version.stderr or version.stdout).strip())
        run([str(javac), "-encoding", "UTF-8", "-d", str(work), str(source)])
        reference = run([str(java), "-cp", str(work), "JavaSemanticsProbe"]).stdout
        actual = run([str(executable), "--vectors"]).stdout
    except (OSError, subprocess.SubprocessError, UnicodeError) as error:
        print(f"FAIL: Java semantics probe could not run: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stdout or "", file=sys.stderr)
            print(error.stderr or "", file=sys.stderr)
        return 1
    (work / "java-vectors.txt").write_text(reference, encoding="utf-8", newline="\n")
    (work / "c-vectors.txt").write_text(actual, encoding="utf-8", newline="\n")
    if actual != reference:
        difference = difflib.unified_diff(reference.splitlines(), actual.splitlines(),
                                          fromfile="java", tofile="C", lineterm="")
        for index, line in enumerate(difference):
            if index == 80:
                print("(remaining differences omitted)", file=sys.stderr)
                break
            print(line, file=sys.stderr)
        print(f"FAIL: vectors differ; full outputs in {work}", file=sys.stderr)
        return 1
    digest = hashlib.sha256(reference.encode("utf-8")).hexdigest()
    print(f"PASS: {len(reference.splitlines())} exact Java/C records; SHA-256 {digest}")
    print(f"C executable SHA-256 {hashlib.sha256(executable.read_bytes()).hexdigest()}")
    print(f"Java probe SHA-256 {hashlib.sha256(source.read_bytes()).hexdigest()}")
    print("Coverage: runtime numeric/RNG primitives only; game call order remains unverified.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
