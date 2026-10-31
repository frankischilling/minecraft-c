"""Run resource contracts in a fresh directory, optionally with a loopback peer."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import threading
from http.server import ThreadingHTTPServer

from http_fixture import Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--http", action="store_true")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work_root = args.work_dir.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="resource-contract-", dir=work_root) as temporary:
        work = Path(temporary).resolve(strict=True)
        if work.parent != work_root:
            raise ValueError("contract directory escaped its selected build root")
        server = None
        worker = None
        try:
            command = [str(executable), str(work)]
            if args.http:
                server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
                worker = threading.Thread(target=server.serve_forever, daemon=True)
                worker.start()
                command.append(f"http://127.0.0.1:{server.server_port}/")
            return subprocess.run(command, cwd=work, timeout=90, check=False).returncode
        finally:
            if server is not None:
                server.shutdown()
                server.server_close()
            if worker is not None:
                worker.join(timeout=5)
                if worker.is_alive():
                    raise RuntimeError("loopback resource server failed to stop")


if __name__ == "__main__":
    raise SystemExit(main())
