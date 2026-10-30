"""Retry one CMake dependency download failure after a transient HTTP response."""
import argparse
import re
import subprocess
import sys
import time


def transient_download(output):
    # A compiler/configuration error or an integrity failure must fail at once.
    # Match CMake's curl diagnostic, not unrelated HTTP text in project output.
    return (re.search(r"The requested URL returned error: (?:429|500|502|503|504)\b", output)
            and re.search(r"Each download failed!|error: downloading|populate-download", output)
            and not re.search(r"hash mismatch|does not match|HASH_MISMATCH", output, re.IGNORECASE))


def run(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    chunks = []
    for chunk in iter(process.stdout.readline, b""):
        sys.stdout.buffer.write(chunk)
        sys.stdout.buffer.flush()
        chunks.append(chunk)
    process.stdout.close()
    return process.wait(), b"".join(chunks).decode("utf-8", errors="replace")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("a CMake configure command is required after --")
    result, output = run(command)
    if result and transient_download(output):
        print("Retrying configuration once after a transient dependency HTTP failure.", flush=True)
        time.sleep(3)
        result, _ = run(command)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
