"""Bound both subprocess streams and preserve failure metadata."""
from contextlib import ExitStack
import subprocess
import threading


def bounded(command, timeout, env, limit, directory=None):
    """Capture at most limit bytes per stream; excess output kills the process."""
    result = {"command": command, "stream_limit_bytes": limit,
              "timeout_seconds": timeout, "timed_out": False, "streams": {}}
    buffers = {}
    with ExitStack() as stack:
        destinations = {}
        for name in ("stdout", "stderr"):
            if directory is None:
                buffers[name] = bytearray()
                destinations[name] = buffers[name].extend
            else:
                destinations[name] = stack.enter_context((directory / (name + ".log")).open("xb")).write
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, env=env)

        def kill():
            try:
                process.kill()
            except ProcessLookupError:
                pass

        def read(name, pipe):
            state = {"observed_bytes": 0, "captured_bytes": 0, "limit_exceeded": False}
            result["streams"][name] = state
            try:
                while True:
                    chunk = pipe.read1(65536)
                    if not chunk:
                        break
                    state["observed_bytes"] += len(chunk)
                    remaining = limit - state["captured_bytes"]
                    prefix = chunk[:remaining]
                    destinations[name](prefix)
                    state["captured_bytes"] += len(prefix)
                    if len(chunk) > remaining:
                        state["limit_exceeded"] = True
                        kill()
                        break
            except Exception as error:
                state["reader_error"] = f"{type(error).__name__}: {error}"
                kill()
            finally:
                pipe.close()

        readers = [threading.Thread(target=read, args=(name, pipe), daemon=True)
                   for name, pipe in (("stdout", process.stdout), ("stderr", process.stderr))]
        for reader in readers:
            reader.start()
        try:
            result["exit"] = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            result["timed_out"] = True
            kill()
            result["exit"] = process.wait(timeout=10)
        for reader in readers:
            reader.join(timeout=10)
        if any(reader.is_alive() for reader in readers):
            # The configured JDK and observer do not create descendant processes.
            # An inherited live pipe nevertheless fails rather than being accepted.
            result["reader_timeout"] = True
    return result, {name: bytes(value) for name, value in buffers.items()}


def successful(result):
    return (result["exit"] == 0 and not result["timed_out"] and
            not result.get("reader_timeout", False) and
            all(not state["limit_exceeded"] and "reader_error" not in state
                for state in result["streams"].values()))
