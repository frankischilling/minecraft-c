"""Real socket image-download controls on authored data; no original host or JAR."""
import argparse
from collections import Counter
import http.server
import json
from pathlib import Path
import queue
import re
import struct
import subprocess
import tempfile
import threading
import time
import zlib


def png(width, height, pixels):
    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body
                + struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff))
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4]
                    for y in range(height))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


RGBA = png(2, 2, bytes((255, 0, 0, 255, 0, 0, 255, 128,
                        1, 2, 3, 0, 0, 255, 0, 255)))
rr_pixels = bytearray(bytes((0x12, 0x34, 0x56, 255)) * (64 * 32))
rr_pixels[(8 * 64 + 40) * 4 + 3] = 127
RR = png(64, 32, rr_pixels)
UNKNOWN = b"authored data with no ImageIO provider\n"
MALFORMED = b"\x89PNG\r\n\x1a\n"


class Peer:
    def __init__(self):
        self.condition = threading.Condition()
        self.requests = Counter()
        self.responses = Counter()
        self.disconnected = Counter()
        self.events = {"gate": threading.Event(), "stop": threading.Event()}
        self.errors = []

    def record(self, table, name):
        with self.condition:
            table[name] += 1
            self.condition.notify_all()

    def wait_requests(self, name, expected):
        with self.condition:
            if not self.condition.wait_for(lambda: self.requests[name] >= expected, timeout=15):
                raise ValueError(f"No real request reached {name}: {dict(self.requests)}")
            if self.requests[name] != expected:
                raise ValueError(f"Unexpected request count for {name}")


def handler_type(peer):
    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def do_GET(self):
            name = self.path.removeprefix("/")
            if name == "http-ok":
                code, body = 200, b"HELLO"
            elif name in ("http-404", "missing", "map-failure"):
                code, body = 404, MALFORMED
            elif name in ("http-500", "server-error"):
                code, body = 500, RGBA
            elif name in ("rgba", "stop"):
                code, body = 200, RGBA
            elif name in ("rr", "gate"):
                code, body = 200, RR
            elif name in ("unknown", "unknown-rr"):
                code, body = 200, UNKNOWN
            else:
                code, body = 418, b"Unexpected test URL"
                peer.errors.append("Unexpected actual request: " + self.path)
            try:
                self.send_response(code)
                self.send_header("Content-Type", "image/png")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.flush()
                # Arrival is published AFTER actual headers. Gated bodies have
                # not supplied even their first PNG byte when C gets its ack.
                peer.record(peer.requests, name)
                gate = peer.events.get(name)
                if gate is not None and not gate.wait(90):
                    peer.errors.append("Body gate never released: " + name)
                    return
                self.wfile.write(body)
                self.wfile.flush()
                peer.record(peer.responses, name)
            except OSError:
                # A source disconnect need not consume a 4xx body; native stop
                # deliberately retires /stop before its peer sends the body.
                peer.record(peer.disconnected, name)

        def log_message(self, *_):
            pass
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="image-download-", dir=args.work_dir.resolve()))
    peer = Peer()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler_type(peer))
    server.daemon_threads = True
    serving = threading.Thread(target=server.serve_forever,
                               kwargs={"poll_interval": 0.05}, daemon=True)
    serving.start()
    base = f"http://127.0.0.1:{server.server_address[1]}"
    command = [str(executable), base]
    output, error = bytearray(), bytearray()
    messages = queue.Queue()
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, cwd=work)

    def collect_stdout():
        for line in iter(process.stdout.readline, b""):
            output.extend(line)
            if len(output) > 1024 * 1024:
                messages.put(ValueError("Native stdout bound"))
                return
            messages.put(line)
        messages.put(None)

    def collect_stderr():
        for block in iter(lambda: process.stderr.read(8192), b""):
            error.extend(block)
            if len(error) > 1024 * 1024:
                messages.put(ValueError("Native stderr bound"))
                return

    readers = [threading.Thread(target=collect_stdout, daemon=True),
               threading.Thread(target=collect_stderr, daemon=True)]
    for reader in readers:
        reader.start()
    protocol = [("SYNC", "gate", 1), ("SYNC", "gate", 2), ("OPEN", "gate", 2),
                ("SYNC", "map-failure", 1), ("SYNC", "stop", 1), ("STOPPED", "stop", 1)]
    received, checks, failure = [], None, None
    started = time.monotonic()
    try:
        while True:
            # This is a hung-protocol deadline, not an elapsed-time success
            # assertion. In particular /stop cannot wait for its 30s HTTP work
            # limit or for this Python peer to release the body as a fallback.
            timeout = 15 if received and received[-1] == ("SYNC", "stop", 1) else 45
            item = messages.get(timeout=timeout)
            if item is None:
                break
            if isinstance(item, Exception):
                raise item
            line = item.decode("ascii").rstrip("\r\n")
            match = re.fullmatch(r"(SYNC|OPEN|STOPPED) ([a-z-]+) ([0-9]+)", line)
            if match:
                step = (match[1], match[2], int(match[3]))
                if len(received) >= len(protocol) or step != protocol[len(received)]:
                    raise ValueError("Unexpected native barrier: " + line)
                action, name, count = step
                peer.wait_requests(name, count)
                if action in ("OPEN", "STOPPED"):
                    if peer.events[name].is_set():
                        raise ValueError("A body gate was released early")
                    peer.events[name].set()
                received.append(step)
                process.stdin.write(b"OK\n")
                process.stdin.flush()
                continue
            match = re.fullmatch(r"Image download contracts: ([1-9][0-9]*) checks", line)
            if match is None or checks is not None or received != protocol:
                raise ValueError("Unexpected native output: " + line)
            checks = int(match[1])
        code = process.wait(timeout=10)
        for reader in readers:
            reader.join(timeout=5)
        expected = Counter({"http-ok": 1, "http-404": 2, "http-500": 1, "rgba": 1,
                            "rr": 1, "unknown": 1, "unknown-rr": 1, "missing": 1,
                            "server-error": 1, "gate": 2, "map-failure": 1, "stop": 1})
        if code != 0 or error or checks is None or received != protocol:
            raise ValueError(f"Native result {code}; checks={checks}; stderr={error!r}")
        if peer.requests != expected or peer.errors:
            raise ValueError(f"Real HTTP requests differ: {dict(peer.requests)}; {peer.errors}")
        # No /no-worker request may be synthesized by a failed Thread.start.
        if peer.requests["no-worker"] != 0:
            raise ValueError("Disabled native worker issued a request")
    except Exception as exc:
        failure = f"{type(exc).__name__}: {exc}"
        raise
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        for event in peer.events.values():
            event.set()
        server.shutdown()
        server.server_close()
        for reader in readers:
            reader.join(timeout=5)
        (work / "stdout.txt").write_bytes(output)
        (work / "stderr.txt").write_bytes(error)
        report = {"status": "FAIL" if failure else "PASS", "failure": failure,
                  "command": command, "exit_code": process.returncode, "checks": checks,
                  "barriers": received, "requests": dict(peer.requests),
                  "responses": dict(peer.responses), "peer_disconnects": dict(peer.disconnected),
                  "peer_errors": peer.errors, "seconds": time.monotonic() - started,
                  "work_directory": str(work), "scope": "Authored loopback; no original host/JAR"}
        (work / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8",
                                          newline="\n")
        print(json.dumps(report))


if __name__ == "__main__":
    main()
