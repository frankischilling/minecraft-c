#!/usr/bin/env python3
"""Capture copied-world original lifecycle calls and their uninstrumented control.

No C process supplies expected values. Original archives remain read-only; all
worlds, isolated helper classes and raw failed captures remain in fresh output.
"""
from __future__ import annotations

import sys
sys.dont_write_bytecode = True

import argparse
import ctypes
import encodings.cp437
import encodings.utf_16_be
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import signal
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]
SOURCE_NAMES = ("ChunkLifecycleObservation.java", "ChunkLifecycleController.java")
JAVA_ENV = ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS", "JDK_JAVAC_OPTIONS", "CLASSPATH")
SPEC = importlib.util.spec_from_file_location("_lifecycle_native", Path(__file__).with_name("compare_lifecycle.py"))
NATIVE = importlib.util.module_from_spec(SPEC)
NATIVE_SOURCE = Path(NATIVE.__file__).resolve()
NATIVE_BYTES = NATIVE_SOURCE.read_bytes()
if len(NATIVE_BYTES) > 2 * 1024 * 1024:
    raise ValueError("bounded native comparison support source")
exec(compile(NATIVE_BYTES, str(NATIVE_SOURCE), "exec"), NATIVE.__dict__)
require, fingerprint, file_fingerprint = NATIVE.require, NATIVE.fingerprint, NATIVE.file_fingerprint


class ProcessTree:
    """Own the launched process group or Windows job, including the JDI child."""
    def __init__(self):
        self.child, self.handle = None, None
        if os.name != "nt":
            require(sys.platform.startswith("linux"), "Linux process ownership required")
            self.libc = ctypes.CDLL(None, use_errno=True)
            self.libc.prctl.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong]
            self.libc.prctl.restype = ctypes.c_int
            previous = ctypes.c_int()
            require(self.libc.prctl(37, ctypes.addressof(previous), 0, 0, 0) == 0, "read prior child subreaper state")
            self.previous_subreaper = previous.value
            require(self.libc.prctl(36, 1, 0, 0, 0) == 0, "own orphaned descendants in isolated producer")
            return
        from ctypes import wintypes as W
        class Basic(ctypes.Structure):
            _fields_ = [("process_time", ctypes.c_longlong), ("job_time", ctypes.c_longlong),
                        ("flags", W.DWORD), ("minimum", ctypes.c_size_t), ("maximum", ctypes.c_size_t),
                        ("processes", W.DWORD), ("affinity", ctypes.c_size_t), ("priority", W.DWORD), ("scheduling", W.DWORD)]
        class Counters(ctypes.Structure):
            _fields_ = [(name, ctypes.c_ulonglong) for name in ("reads", "writes", "other", "read_bytes", "write_bytes", "other_bytes")]
        class Limits(ctypes.Structure):
            _fields_ = [("basic", Basic), ("io", Counters), ("process_memory", ctypes.c_size_t),
                        ("job_memory", ctypes.c_size_t), ("peak_process", ctypes.c_size_t), ("peak_job", ctypes.c_size_t)]
        class Accounting(ctypes.Structure):
            _fields_ = [(name, ctypes.c_longlong) for name in ("user", "kernel", "period_user", "period_kernel")] + [
                (name, W.DWORD) for name in ("faults", "total", "active", "terminated")]
        self.Accounting = Accounting
        class ThreadEntry(ctypes.Structure):
            _fields_ = [("size", W.DWORD), ("usage", W.DWORD), ("thread", W.DWORD), ("owner", W.DWORD),
                        ("base_priority", W.LONG), ("delta_priority", W.LONG), ("flags", W.DWORD)]
        self.ThreadEntry = ThreadEntry
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        signatures = {
            "CreateJobObjectW": ([ctypes.c_void_p, W.LPCWSTR], W.HANDLE),
            "SetInformationJobObject": ([W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD], W.BOOL),
            "AssignProcessToJobObject": ([W.HANDLE, W.HANDLE], W.BOOL),
            "TerminateJobObject": ([W.HANDLE, W.UINT], W.BOOL),
            "QueryInformationJobObject": ([W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD, ctypes.c_void_p], W.BOOL),
            "OpenProcess": ([W.DWORD, W.BOOL, W.DWORD], W.HANDLE),
            "CreateToolhelp32Snapshot": ([W.DWORD, W.DWORD], W.HANDLE),
            "Thread32First": ([W.HANDLE, ctypes.POINTER(ThreadEntry)], W.BOOL),
            "Thread32Next": ([W.HANDLE, ctypes.POINTER(ThreadEntry)], W.BOOL),
            "OpenThread": ([W.DWORD, W.BOOL, W.DWORD], W.HANDLE),
            "ResumeThread": ([W.HANDLE], W.DWORD),
            "CloseHandle": ([W.HANDLE], W.BOOL)}
        for name, (args, result) in signatures.items():
            function = getattr(kernel, name)
            function.argtypes, function.restype = args, result
        self.kernel = kernel
        self.handle = kernel.CreateJobObjectW(None, None)
        require(self.handle, "create private process job")
        limits = Limits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
        if not kernel.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            kernel.CloseHandle(self.handle)
            self.handle = None
            raise OSError(ctypes.get_last_error(), "set private process job limits")

    def launch(self, command, **kwargs):
        # A suspended Windows primary thread cannot spawn a JDI child before
        # job assignment. CPython closes its original thread handle, so reopen
        # only the single primary thread belonging to this still-suspended PID.
        self.child = subprocess.Popen(command, start_new_session=os.name != "nt",
                                      creationflags=0x4 if os.name == "nt" else 0, **kwargs)
        if self.handle is not None:
            process = self.kernel.OpenProcess(0x100 | 0x1, False, self.child.pid)
            try:
                require(process and self.kernel.AssignProcessToJobObject(self.handle, process), "assign original process to private job")
                snapshot = self.kernel.CreateToolhelp32Snapshot(0x4, 0)
                require(snapshot and snapshot != ctypes.c_void_p(-1).value, "snapshot suspended primary thread")
                try:
                    entry = self.ThreadEntry()
                    entry.size = ctypes.sizeof(entry)
                    found, scanned = [], 0
                    present = self.kernel.Thread32First(snapshot, ctypes.byref(entry))
                    while present:
                        scanned += 1
                        require(scanned <= 131072, "bounded thread inventory")
                        if entry.owner == self.child.pid:
                            found.append(entry.thread)
                        entry.size = ctypes.sizeof(entry)
                        present = self.kernel.Thread32Next(snapshot, ctypes.byref(entry))
                    require(ctypes.get_last_error() == 18 and len(found) == 1,
                            "complete inventory with exactly one suspended primary thread")
                    thread = self.kernel.OpenThread(0x2, False, found[0])
                    require(thread, "open own suspended primary thread")
                    try:
                        require(self.kernel.ResumeThread(thread) == 1, "resume assigned primary thread exactly once")
                    finally:
                        self.kernel.CloseHandle(thread)
                finally:
                    self.kernel.CloseHandle(snapshot)
            except BaseException:
                self.child.kill()
                self.child.wait(timeout=10)
                raise
            finally:
                if process:
                    self.kernel.CloseHandle(process)
        return self.child

    def terminate(self):
        if self.child is None:
            return
        if self.handle is not None:
            require(self.kernel.TerminateJobObject(self.handle, 1), "terminate owned original process job")
        else:
            try:
                os.killpg(self.child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass

    def close(self):
        if self.handle is None:
            remaining, reaped = False, []
            try:
                if self.child is not None:
                    try:
                        os.killpg(self.child.pid, 0)
                        remaining = True
                    except ProcessLookupError:
                        pass
                    if remaining:
                        self.terminate()
                    deadline = time.monotonic() + 10
                    while True:
                        try:
                            pid, status = os.waitpid(-self.child.pid, os.WNOHANG)
                        except ChildProcessError:
                            break
                        if pid:
                            reaped.append({"pid": pid, "wait_status": status})
                        else:
                            require(time.monotonic() < deadline, "owned adopted descendants cleanup deadline")
                            time.sleep(0.02)
                    try:
                        os.killpg(self.child.pid, 0)
                    except ProcessLookupError:
                        pass
                    else:
                        raise NATIVE.Failure("owned process group remains after adopted descendant reaping")
                return {"kind": "process-group", "direct_child_reaped": self.child is None or self.child.poll() is not None,
                        "remaining_group_terminated": remaining, "adopted_descendants_reaped": reaped,
                        "prior_subreaper": self.previous_subreaper}
            finally:
                require(self.libc.prctl(36, self.previous_subreaper, 0, 0, 0) == 0, "restore prior isolated-producer subreaper state")
        try:
            account = self.Accounting()
            require(self.kernel.QueryInformationJobObject(self.handle, 1, ctypes.byref(account), ctypes.sizeof(account), None),
                    "query owned process job accounting")
            remaining = account.active
            if account.active:
                self.terminate()
                deadline = time.monotonic() + 10
                while account.active and time.monotonic() < deadline:
                    time.sleep(0.02)
                    require(self.kernel.QueryInformationJobObject(self.handle, 1, ctypes.byref(account), ctypes.sizeof(account), None),
                            "query terminated process job")
            require(account.active == 0, "all owned Windows processes exited")
            return {"kind": "windows-job", "processes": account.total, "active_after_cleanup": account.active,
                    "remaining_processes_terminated": remaining}
        finally:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def live_artifacts(directory):
    count, total = 0, 0
    for path in directory.rglob("*"):
        count += 1
        require(count <= 2048, "live artifact path bound")
        try:
            require(NATIVE.ordinary(path), "ordinary live capture artifact")
            if path.is_file():
                size = path.stat().st_size
                require(size <= NATIVE.MAX_RECORD, "live artifact byte bound")
                total += size
                require(total <= 512 * 1024 * 1024, "complete live artifact byte bound")
            else:
                require(path.is_dir(), "ordinary live artifact directory")
        except FileNotFoundError:
            pass  # Atomic metadata/record rotation is allowed during a poll.


def run(name, command, directory, report, seconds):
    out, err = directory / (name + ".stdout"), directory / (name + ".stderr")
    row = {"name": name, "command": list(map(str, command)), "cwd": str(directory), "seconds_limit": seconds,
           "per_stream_limit": NATIVE.MAX_RECORD, "returncode": None, "stopped": None, "reaped": False}
    report["processes"].append(row)
    tree, child, started = ProcessTree(), None, time.monotonic()
    next_inventory, primary = started, None
    output = errors = None
    try:
        output = out.open("xb")
        errors = err.open("xb")
        child = tree.launch(row["command"], cwd=directory, stdin=subprocess.DEVNULL, stdout=output, stderr=errors)
        row["pid"] = child.pid
        while child.poll() is None:
            if time.monotonic() - started >= seconds:
                row["stopped"] = "timeout"
                tree.terminate()
                break
            if out.stat().st_size > NATIVE.MAX_RECORD or err.stat().st_size > NATIVE.MAX_RECORD:
                row["stopped"] = "stream-limit"
                tree.terminate()
                break
            if time.monotonic() >= next_inventory:
                try:
                    live_artifacts(directory)
                except (NATIVE.Failure, OSError) as error:
                    row["stopped"] = "artifact-limit: " + str(error)
                    tree.terminate()
                    break
                next_inventory = time.monotonic() + 0.25
            time.sleep(0.02)
    except BaseException as error:
        primary = error
    finally:
        cleanup = []
        child = child or tree.child
        if child is not None:
            try:
                if child.poll() is None:
                    tree.terminate()
                row["returncode"] = child.wait(timeout=10)
                row["reaped"] = True
            except BaseException as error:
                cleanup.append(error)
        try:
            row["process_tree"] = tree.close()
        except BaseException as error:
            cleanup.append(error)
        # All inherited writers have exited before the final size measurement.
        # Failed streams remain bounded and their observed totals remain visible.
        for label, stream in (("stdout", output), ("stderr", errors)):
            if stream is not None:
                try:
                    size = os.fstat(stream.fileno()).st_size
                    row[label + "_total_bytes"] = size
                    if size > NATIVE.MAX_RECORD:
                        row["stopped"] = row["stopped"] or "stream-limit"
                    stream.truncate(min(size, NATIVE.MAX_RECORD))
                except BaseException as error:
                    cleanup.append(error)
                finally:
                    try:
                        stream.close()
                    except BaseException as error:
                        cleanup.append(error)
        row["seconds"] = round(time.monotonic() - started, 6)
        for label, path in (("stdout", out), ("stderr", err)):
            try:
                if path.is_file():
                    row[label] = {"path": str(path), **file_fingerprint(path)}
            except BaseException as error:
                cleanup.append(error)
        try:
            live_artifacts(directory)
        except (NATIVE.Failure, OSError) as error:
            row["stopped"] = row["stopped"] or "artifact-limit: " + str(error)
            cleanup.append(error)
        if cleanup:
            row["cleanup_errors"] = [type(error).__name__ + ": " + str(error) for error in cleanup]
            primary = primary or cleanup[0]
    if primary is not None:
        raise primary
    require(row["returncode"] == 0 and row["stopped"] is None and
            not row["process_tree"].get("remaining_group_terminated", False) and
            not row["process_tree"].get("remaining_processes_terminated", 0),
            "unsuccessful bounded original process: " + name)


def jdk_inputs(java, javac):
    result = {java, javac}
    require(java.parent == javac.parent, "same runtime/compiler JDK")
    root = java.parent.parent
    names = ("release", "lib/modules", "lib/ct.sym", "jmods/java.base.jmod")
    native = (("bin/server/jvm.dll", "bin/java.dll", "bin/jli.dll", "bin/jimage.dll", "bin/zip.dll", "bin/jdwp.dll", "bin/dt_socket.dll")
              if os.name == "nt" else ("lib/server/libjvm.so", "lib/libjava.so", "lib/libjli.so", "lib/libjimage.so",
                                       "lib/libzip.so", "lib/libverify.so", "lib/libjdwp.so", "lib/libdt_socket.so"))
    for name in (*names, *native):
        path = root / name
        require(path.is_file(), "required JDK provenance input: " + name)
        result.add(path)
    release = (root / "release").read_text(encoding="utf-8")
    match = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
    require(match and int(match.group(1)) >= 17, "JDK17 or newer required for lifecycle observation")
    return result


def compiled_classes(directory):
    rows = {}
    for path in sorted(directory.rglob("*")):
        require(NATIVE.ordinary(path), "ordinary isolated compiler output")
        if path.is_file():
            raw = NATIVE.read(path, 2 * 1024 * 1024)
            require(path.suffix == ".class" and len(raw) >= 8 and raw[:4] == b"\xca\xfe\xba\xbe" and
                    struct.unpack_from(">H", raw, 6)[0] == 61, "actual release17 helper class")
            rows[path.relative_to(directory).as_posix()] = fingerprint(raw)
    require(set(rows) == {"ChunkLifecycleObservation.class", "ChunkLifecycleObservation$Observer.class",
                          "ChunkLifecycleObservation$SetupUnavailable.class", "ChunkLifecycleController.class",
                          "ChunkLifecycleController$Session.class"}, "exact isolated helper class inventory")
    return rows


def python_modules():
    """Measure loaded Python files, caches and the interpreter's own library.

    These are the files exposed by this interpreter, not an operating-system
    dependency closure. A source and an existing bytecode cache are both kept;
    neither is silently substituted for the other in the recorded inventory.
    """
    if os.name == "nt":
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetModuleFileNameW.argtypes = [wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
        kernel.GetModuleFileNameW.restype = wintypes.DWORD
        buffer = ctypes.create_unicode_buffer(32768)
        size = kernel.GetModuleFileNameW(sys.dllhandle, buffer, len(buffer))
        require(0 < size < len(buffer), "actual Python interpreter library path")
        library = Path(buffer.value).resolve()
    else:
        # An embedded Linux interpreter can have an optional, unused shared
        # library installed. Only its actual current mappings establish use.
        maps = NATIVE.read(Path("/proc/self/maps"), 2 * 1024 * 1024).decode("utf-8")
        libraries = set()
        for line in maps.splitlines():
            parts = line.split(None, 5)
            if len(parts) == 6 and re.fullmatch(r"libpython[0-9]+\.[0-9]+(?:t)?\.so(?:\.[0-9]+)*", Path(parts[5]).name):
                libraries.add(Path(parts[5]).resolve())
        require(len(libraries) <= 1, "one actual mapped Python interpreter library")
        library = next(iter(libraries), None)
    rows, files = {}, {Path(sys.executable).resolve()}
    if library is not None:
        files.add(library)
    for name, module in sorted(list(sys.modules.items())):
        require(len(rows) < 512, "loaded Python module count bound")
        if module is None:
            continue
        source = getattr(module, "__file__", None)
        cached = getattr(module, "__cached__", None)
        row = {"origin": getattr(getattr(module, "__spec__", None), "origin", None),
               "file": None, "cache": None}
        for key, value in (("file", source), ("cache", cached)):
            if value is not None:
                path = Path(value).resolve()
                if key == "cache" and not path.exists():
                    continue
                require(path.is_file() and NATIVE.ordinary(path), "ordinary loaded Python module input")
                row[key] = str(path)
                files.add(path)
        rows[name] = row
    return {"modules": rows, "interpreter_library": str(library) if library is not None else None,
            "interpreter_linkage": "shared" if library is not None else "embedded"}, files


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "java", "javac", "work-dir"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--context", choices=("client", "base", "dedicated"), required=True)
    parser.add_argument("--scenario", choices=("cohort", "unload101", "session-world", "detach-owned"), required=True)
    args = parser.parse_args(argv)
    report = {"schema": 1, "scope": "original-lifecycle-controlled-capture", "status": "running",
              "tested_commit": None, "processes": [], "errors": [], "C_executed": False}
    directory, inputs = None, {}
    try:
        require(not any(os.environ.get(name) for name in JAVA_ENV), "injected Java environment")
        java, javac, client, server = (path.resolve() for path in (args.java, args.javac, args.client, args.server))
        require(args.scenario != "unload101" or args.context == "dedicated", "natural nonempty pending requires dedicated context")
        sources = [ROOT / "tests/reference/cache" / name for name in SOURCE_NAMES]
        controls_path = Path(__file__).with_name("lifecycle_controls.py")
        controls_before = file_fingerprint(controls_path)
        controls = NATIVE.load("_lifecycle_control_validation", controls_path, expected=controls_before)
        # The validator binds original class URLs and decodes Java UTF-16 units.
        # Warm the interpreter's actual URI/codec import paths before freezing
        # the module inventory, including Python versions with lazy URL imports.
        client.as_uri()
        server.as_uri()
        "".encode("utf-16-be")
        modules_before, module_files = python_modules()
        report["python_modules_before"] = modules_before
        paths = {*sources, Path(__file__).resolve(), Path(NATIVE.__file__).resolve(), controls_path,
                 Path(sys.executable).resolve(), client, server, *jdk_inputs(java, javac), *module_files}
        directory = NATIVE.fresh_work(args.work_dir, (java.parent.parent, client, server,
                                   Path(sys.executable).resolve().parent, *module_files))
        inputs = {str(path): file_fingerprint(path) for path in sorted(paths)}
        report["inputs_before"] = inputs
        require(inputs[str(controls_path)] == controls_before, "executed controls source matches measured input")
        require(inputs[str(NATIVE_SOURCE)] == fingerprint(NATIVE_BYTES), "executed support source matches measured input")
        require(inputs[str(client)]["sha256"] == NATIVE.PIN["client"] and inputs[str(server)]["sha256"] == NATIVE.PIN["base"],
                "both supplied original archive pins")
        report["python"] = {"executable": sys.executable, "version": sys.version,
                            "path": list(sys.path), "flags": repr(sys.flags),
                            "environment": {name: os.environ.get(name) for name in
                                            ("PYTHONHOME", "PYTHONPATH", "PYTHONSAFEPATH", "PYTHONNOUSERSITE", "PYTHONDONTWRITEBYTECODE")},
                            "scope": "loaded Python files/caches/extensions and interpreter library; no OS dependency closure"}
        for name in ("source", "classes", "empty", "compile-work"):
            (directory / name).mkdir()
        for source in sources:
            shutil.copyfile(source, directory / "source" / source.name)
            require(file_fingerprint(directory / "source" / source.name) == inputs[str(source)], "exact isolated helper source")
        for source in (Path(__file__).resolve(), Path(NATIVE.__file__).resolve(), controls_path):
            shutil.copyfile(source, directory / "source" / source.name)
        run("java-version", [java, "-Xmx128m", "-version"], directory, report, 30)
        run("compile", [javac, "-J-Xmx512m", "-proc:none", "-implicit:none", "-Xlint:all", "-Werror", "--release", "17",
                        "--add-modules", "jdk.jdi", "-encoding", "UTF-8", "-classpath", directory / "empty", "-sourcepath", directory / "empty",
                        "--module-path", directory / "empty", "-d", directory / "classes", *[directory / "source" / name for name in SOURCE_NAMES]],
            directory, report, 60)
        report["classes_before"] = compiled_classes(directory / "classes")
        preparation = directory / "preparation"
        preparation.mkdir()
        jar = client if args.context == "client" else server
        preparation_context = "client" if args.context == "client" else "base"
        preparation_mode = "prepare-detach" if args.scenario == "detach-owned" else "prepare"
        run("prepare", [java, "-Xint", "-Xmx512m", "-cp", directory / "classes", "ChunkLifecycleObservation", jar,
                        preparation_context, preparation_mode, "0000000000000017", preparation, directory / "preparation.json"], directory, report, 180)
        report["preparation"] = {"snapshot": file_fingerprint(directory / "preparation.json"), "files": NATIVE.inventory(preparation)}
        for mode in ("observed", "control"):
            world = directory / (mode + "-world")
            before = NATIVE.inventory(preparation)
            shutil.copytree(preparation, world)
            report[mode + "_before"] = NATIVE.inventory(world)
            require(before == report[mode + "_before"] == NATIVE.inventory(preparation), "unchanged fresh original baseline copy")
            arguments = [jar, args.context, args.scenario, "0000000000000017", world, directory / (mode + ".json")]
            command = ([java, "--add-modules", "jdk.jdi", "-Xmx512m", "-cp", directory / "classes", "ChunkLifecycleController",
                        *arguments, directory / "trace.json"] if mode == "observed" else
                       [java, "-Xint", "-Xmx512m", "-cp", directory / "classes", "ChunkLifecycleObservation", *arguments])
            run(mode, command, directory, report, 360 if mode == "observed" else 180)
            require((directory / (mode + ".json")).stat().st_size <= 32 * 1024 * 1024, "bounded complete helper snapshot")
            report[mode + "_snapshot"] = file_fingerprint(directory / (mode + ".json"))
            report[mode + "_after"] = NATIVE.inventory(world)
            if mode == "observed":
                require((directory / "trace.json").stat().st_size <= NATIVE.MAX_RECORD, "bounded complete original trace")
                report["trace_snapshot"] = file_fingerprint(directory / "trace.json")
        report["status"] = "captured-pending-validation"
    except Exception as error:
        report["status"] = "fail"
        report["errors"].append(type(error).__name__ + ": " + str(error))
    finally:
        if directory is not None:
            try:
                report["retained_worlds"] = {name: NATIVE.inventory(directory / name) for name in
                                            ("preparation", "observed-world", "control-world") if (directory / name).is_dir()}
                report["inputs_after"] = {path: file_fingerprint(Path(path)) for path in inputs}
                require(report["inputs_after"] == inputs, "all measured original/tool/source inputs preserved")
                report["python_modules_after"], _ = python_modules()
                require(report["python_modules_after"] == report["python_modules_before"], "loaded Python module inventory preserved")
                if "classes_before" in report:
                    report["classes_after"] = compiled_classes(directory / "classes")
                    require(report["classes_after"] == report["classes_before"], "isolated helper classes preserved")
            except Exception as error:
                report["status"] = "fail"
                report["errors"].append(type(error).__name__ + ": " + str(error))
            manifest = directory / "manifest.json"
            manifest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            if report["status"] == "captured-pending-validation":
                try:
                    validation = controls.validate_capture(directory)
                    require(all(file_fingerprint(Path(path)) == before for path, before in inputs.items()),
                            "source/tool/archive inputs preserved through control validation")
                    report["python_modules_after"], _ = python_modules()
                    require(report["python_modules_after"] == report["python_modules_before"], "Python modules preserved through control validation")
                    (directory / "control-validation.json").write_text(json.dumps(validation, indent=2) + "\n", encoding="utf-8", newline="\n")
                    report["status"] = "pass"
                except Exception as error:
                    report["status"] = "fail"
                    report["errors"].append(type(error).__name__ + ": " + str(error))
                manifest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print(json.dumps({"status": report["status"], "manifest": str(manifest), "errors": report["errors"]}))
        else:
            print(json.dumps({"status": "fail", "errors": report["errors"]}))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
