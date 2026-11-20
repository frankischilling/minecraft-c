"""Bounded process ownership and file records for dedicated observations."""
from __future__ import annotations

from contextlib import ExitStack
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import time

ROOT = Path(__file__).resolve().parents[5]
OPTIONS = ("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "_JAVA_OPTIONS", "CLASSPATH")
MIB = 1024 * 1024
CLEANUP_SECONDS = 5
INVENTORY_ENTRIES = 4096
INVENTORY_BYTES = 256 * MIB
MAX_INPUT_BYTES = 512 * MIB

def need(condition, message):
    if not condition:
        raise ValueError(message)


def ordinary(path):
    """Reject symlinks/reparse points before resolving any existing ancestor."""
    path = Path(path).absolute()
    for item in (path, *path.parents):
        if item.exists() or item.is_symlink():
            info = item.lstat()
            need(not stat.S_ISLNK(info.st_mode) and
                 not (getattr(info, "st_file_attributes", 0) & 0x400),
                 "nonordinary path: " + str(item))
    return path


def fingerprint(path):
    path = ordinary(path)
    need(path.is_file(), "missing input/file: " + str(path))
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(MIB), b""):
            digest.update(block)
            size += len(block)
            need(size <= MAX_INPUT_BYTES, "input/file fingerprint byte bound: " + str(path))
    return {"bytes": size, "sha256": digest.hexdigest()}


def input_snapshot(paths):
    return {name: {"path": str(path.absolute()), **fingerprint(path)}
            for name, path in paths.items()}


def inventory(directory):
    """Include the root and every empty directory as well as every file."""
    directory = ordinary(directory)
    need(directory.is_dir(), "missing inventory root: " + str(directory))
    result = {".": {"kind": "directory"}}
    total = 0
    for current, directories, filenames in os.walk(directory, followlinks=False):
        directories.sort()
        filenames.sort()
        for name in (*directories, *filenames):
            path = ordinary(Path(current) / name)
            relative = path.relative_to(directory).as_posix()
            need(len(result) < INVENTORY_ENTRIES, "inventory entry bound")
            if path.is_dir():
                result[relative] = {"kind": "directory"}
            else:
                need(path.is_file(), "unsupported retained file type")
                info = fingerprint(path)
                total += info["bytes"]
                need(total <= INVENTORY_BYTES, "inventory cumulative byte bound")
                result[relative] = {"kind": "file", **info}
    return result


def environment():
    need(all(not os.environ.get(name) for name in OPTIONS), "injected Java option/classpath environment")
    result = os.environ.copy()
    for name in OPTIONS:
        result.pop(name, None)
    # The journal uses explicit immutable objects and the actual repository.
    need(all(not os.environ.get(name) for name in (
        "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_OBJECT_DIRECTORY",
        "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_CONFIG", "GIT_CONFIG_COUNT")),
        "injected Git repository/config environment")
    result["GIT_OPTIONAL_LOCKS"] = "0"
    result["GIT_TERMINAL_PROMPT"] = "0"
    result["GIT_PAGER"] = "cat"
    return result


def launch(command, work, name, receipts, files, children, env, stdout_limit, stderr_limit,
           *, receipt_root=None, receipt_name=None):
    command = [str(part) for part in command]
    outgoing, errors = work / (name + ".stdout"), work / (name + ".stderr")
    stdout = files.enter_context(outgoing.open("xb"))
    stderr = files.enter_context(errors.open("xb"))
    receipt = {
        "name": receipt_name or name, "command": command, "cwd": str(ROOT), "stdin": "DEVNULL",
        "pid": None, "exit_code": None, "reaped": False, "kill_requested": False,
        "timed_out": False, "output_limited": False, "natural_exit": False,
        "stdout_path": outgoing.relative_to(receipt_root or work).as_posix(),
        "stderr_path": errors.relative_to(receipt_root or work).as_posix(),
        "stdout_limit": stdout_limit, "stderr_limit": stderr_limit,
        "started_unix_ns": time.time_ns(),
    }
    receipts.append(receipt)
    try:
        child = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                                 stdout=stdout, stderr=stderr, env=env)
    except BaseException as error:
        receipt["launch_error"] = f"{type(error).__name__}: {error}"
        receipt["stdout"] = fingerprint(outgoing)
        receipt["stderr"] = fingerprint(errors)
        raise
    receipt["pid"] = child.pid
    owned = {
        "child": child, "receipt": receipt, "stdout": outgoing, "stderr": errors,
        "started": time.monotonic(),
    }
    children.append(owned)
    return owned


def stream_bounds(children):
    for owned in children:
        receipt = owned["receipt"]
        exceeded = (owned["stdout"].stat().st_size > receipt["stdout_limit"] or
                    owned["stderr"].stat().st_size > receipt["stderr_limit"])
        receipt["output_limited"] |= exceeded
        need(not exceeded, "process output threshold: " + receipt["name"])


def cleanup(children, failure):
    """Kill every live owned child before waiting for either child."""
    for owned in reversed(children):
        child, receipt = owned["child"], owned["receipt"]
        try:
            if child.poll() is None:
                receipt["kill_requested"] = True
                child.kill()
        except BaseException as error:
            receipt["termination_error"] = f"{type(error).__name__}: {error}"
            if failure is None:
                failure = error
    for owned in reversed(children):
        child, receipt = owned["child"], owned["receipt"]
        try:
            child.wait(timeout=CLEANUP_SECONDS)
        except BaseException as error:
            receipt["wait_error"] = f"{type(error).__name__}: {error}"
            if failure is None:
                failure = error
        receipt.update(exit_code=child.returncode, reaped=child.returncode is not None,
                       seconds=round(time.monotonic() - owned["started"], 6),
                       ended_unix_ns=time.time_ns())
        receipt["natural_exit"] = (receipt["reaped"] and not receipt["kill_requested"] and
                                   "termination_error" not in receipt and "wait_error" not in receipt)
    # Never let one hashing failure skip the other child's wait or journal.
    for owned in children:
        receipt = owned["receipt"]
        try:
            receipt["stdout"] = fingerprint(owned["stdout"])
            receipt["stderr"] = fingerprint(owned["stderr"])
            receipt["output_limited"] |= (receipt["stdout"]["bytes"] > receipt["stdout_limit"] or
                                          receipt["stderr"]["bytes"] > receipt["stderr_limit"])
        except BaseException as error:
            receipt["journal_error"] = f"{type(error).__name__}: {error}"
            if failure is None:
                failure = error
    return failure


def run(command, work, name, receipts, env, seconds=60, stdout_limit=4 * MIB, stderr_limit=4 * MIB,
        *, receipt_root=None, receipt_name=None, watched_outputs=None):
    children = []
    failure = None
    with ExitStack() as files:
        try:
            owned = launch(command, work, name, receipts, files, children, env,
                           stdout_limit, stderr_limit, receipt_root=receipt_root,
                           receipt_name=receipt_name)
            child, receipt = owned["child"], owned["receipt"]
            receipt["deadline_seconds"] = seconds
            while True:
                status = child.poll()
                stream_bounds(children)
                for path, limit in (watched_outputs or {}).items():
                    path = ordinary(path)
                    if path.exists():
                        need(path.is_file() and path.stat().st_size <= limit,
                             "process output file threshold: " + str(path))
                if status is not None:
                    break
                if time.monotonic() - owned["started"] >= seconds:
                    receipt["timed_out"] = True
                    raise TimeoutError("process deadline: " + name)
                time.sleep(0.02)
        except BaseException as error:
            failure = error
        finally:
            failure = cleanup(children, failure)
    if failure is not None:
        raise failure
    receipt = children[0]["receipt"]
    need(receipt["natural_exit"] and receipt["exit_code"] == 0 and
         not receipt["output_limited"], "unsuccessful process: " + name)
    return (work / (name + ".stdout")).read_bytes(), (work / (name + ".stderr")).read_bytes()
