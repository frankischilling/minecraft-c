"""Hash a complete authored large build artifact without changing resource limits."""
import hashlib
import importlib.util
import os
from pathlib import Path
import tempfile
import tracemalloc


def need(condition, message):
    if not condition:
        raise AssertionError(message)


def make_sparse(stream):
    if os.name != "nt":
        return
    import ctypes
    from ctypes import wintypes
    import msvcrt

    control = ctypes.WinDLL("kernel32", use_last_error=True).DeviceIoControl
    control.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD,
                        wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
                        wintypes.LPVOID]
    control.restype = wintypes.BOOL
    enabled = ctypes.c_ubyte(1)  # FILE_SET_SPARSE_BUFFER.SetSparse, Windows SDK winioctl.h.
    returned = wintypes.DWORD()
    if not control(msvcrt.get_osfhandle(stream.fileno()), 0x900c4, ctypes.byref(enabled),
                   ctypes.sizeof(enabled), None, 0, ctypes.byref(returned), None):
        raise ctypes.WinError(ctypes.get_last_error())


def rejected(action, expected):
    try:
        action()
    except AssertionError as error:
        need(expected in str(error), "specific bounded-input failure: " + str(error))
    else:
        raise AssertionError("expected rejection: " + expected)


def main():
    path = Path(__file__).with_name("test_startup.py")
    spec = importlib.util.spec_from_file_location("startup_fingerprint_subject", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    need(module.FILE_LIMIT == 128 * 1024 * 1024, "resource file limit unchanged")
    need(module.PNG_LIMIT == 16 * 1024 * 1024, "PNG limit unchanged")
    prefix = b"authored executable fingerprint fixture\x00"
    suffix = b"complete artifact tail\xff"
    size = 128 * 1024 * 1024 + 257
    with tempfile.TemporaryDirectory(prefix="mc-startup-fingerprint-") as directory:
        # Match the real Windows call site: Path.stat infers executable bits
        # from .exe while descriptor stat has no filename for that inference.
        binary = Path(directory) / "authored-build-artifact.exe"
        with binary.open("wb") as stream:
            make_sparse(stream)
            need(stream.write(prefix) == len(prefix), "complete fixture prefix")
            stream.seek(size - len(suffix))
            need(stream.write(suffix) == len(suffix), "complete fixture suffix")
        need(binary.stat().st_size == size, "authored sparse artifact size")
        expected = hashlib.sha256(prefix)
        gap = size - len(prefix) - len(suffix)
        zeros = bytes(65536)
        while gap:
            count = min(gap, len(zeros))
            expected.update(zeros[:count])
            gap -= count
        expected.update(suffix)
        tracemalloc.start()
        try:
            observed = module.executable_pin(binary)
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        need(observed == {"bytes": size, "sha256": expected.hexdigest()},
             "full large executable hash includes its final nonzero bytes")
        need(peak < 4 * 1024 * 1024, "executable hash uses bounded Python buffers")
        need(module.executable_pin(binary) == observed, "stable before/after executable identity")
        rejected(lambda: module.raw_file(binary), f"bytes={size}, limit={module.FILE_LIMIT}")
        rejected(lambda: module.raw_file(binary, module.PNG_LIMIT),
                 f"bytes={size}, limit={module.PNG_LIMIT}")
        rejected(lambda: module.executable_pin(Path(directory)), "mode=d")
        oversized = Path(directory) / "over-executable-limit"
        with oversized.open("wb") as stream:
            make_sparse(stream)
            stream.truncate(module.EXECUTABLE_LIMIT + 1)
        rejected(lambda: module.executable_pin(oversized),
                 f"bytes={module.EXECUTABLE_LIMIT + 1}, limit={module.EXECUTABLE_LIMIT}")
    print("Startup executable fingerprint: complete sparse artifact over128MiB; resource/PNG bounds retained")


if __name__ == "__main__":
    main()
