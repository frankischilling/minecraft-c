#!/usr/bin/env python3
"""Launch the original offline server on loopback in a new disposable directory."""

import argparse
import hashlib
import json
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import threading
import time

SERVER_SHA256 = "033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path,
                        help="New directory; existing paths are refused")
    parser.add_argument("--java", default="java")
    parser.add_argument("--port", type=int, default=25575)
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    if not args.jar.is_file() or not shutil.which(args.java):
        print("UNAVAILABLE: supplied server JAR or Java runtime is missing", file=sys.stderr)
        return 77
    if not 1024 <= args.port <= 65535 or not 1 <= args.timeout <= 600:
        parser.error("port must be 1024..65535 and timeout 1..600 seconds")
    jar = args.jar.resolve()
    if hashlib.sha256(jar.read_bytes()).hexdigest() != SERVER_SHA256:
        parser.error("server JAR fingerprint differs from the verified reference")
    if args.work_dir.exists() or args.work_dir.is_symlink():
        parser.error("work directory already exists; choose a fresh directory")
    args.work_dir.mkdir(parents=True)
    (args.work_dir / "server.properties").write_text(
        "server-ip=127.0.0.1\n"
        f"server-port={args.port}\n"
        "online-mode=false\nlevel-name=smoke-world\nlevel-seed=17\n"
        "allow-nether=false\nspawn-monsters=false\nspawn-animals=false\n"
        "max-players=2\nview-distance=3\n", encoding="ascii")
    command = [args.java, "-Djava.awt.headless=true", "-Xms128M", "-Xmx768M",
               "-cp", str(jar), "net.minecraft.server.MinecraftServer", "nogui"]
    lines = queue.Queue()
    transcript = []
    start = time.monotonic()
    ready = False
    timed_out = False
    process = subprocess.Popen(command, cwd=args.work_dir, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding="utf-8", errors="replace")

    def read_output():
        for line in process.stdout:
            lines.put(line)
        lines.put(None)

    reader = threading.Thread(target=read_output, daemon=True)
    reader.start()
    try:
        while True:
            if time.monotonic() - start > args.timeout:
                timed_out = True
                break
            try:
                line = lines.get(timeout=0.25)
            except queue.Empty:
                if process.poll() is not None:
                    break
                continue
            if line is None:
                break
            transcript.append(line)
            print(line, end="", flush=True)
            if "Done (" in line and not ready:
                ready = True
                process.stdin.write("list\nsave-all\nstop\n")
                process.stdin.flush()
        if process.poll() is None:
            if not ready:
                process.stdin.write("stop\n")
                process.stdin.flush()
            process.wait(timeout=30)
    except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
        timed_out = True
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        reader.join(timeout=5)
        while not lines.empty():
            line = lines.get_nowait()
            if line is not None:
                transcript.append(line)
        process.stdin.close()
        process.stdout.close()
    output = "".join(transcript)
    passed = (ready and not timed_out and process.returncode == 0
              and "Stopping server" in output and "Saving chunks" in output
              and "CONSOLE: Save complete." in output
              and (args.work_dir / "smoke-world" / "level.dat").is_file())
    result = {"server_sha256": SERVER_SHA256, "command": command,
              "bind": f"127.0.0.1:{args.port}", "seed": 17,
              "ready": ready, "timed_out": timed_out,
              "exit_code": process.returncode, "passed": passed,
              "scope": "original server startup/save/stop only; no client session"}
    (args.work_dir / "transcript.txt").write_text(output, encoding="utf-8")
    (args.work_dir / "result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("PASS: original loopback server started, saved, and stopped" if passed
          else "FAIL: inspect local transcript.txt and result.json")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
