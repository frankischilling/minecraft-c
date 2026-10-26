"""Use the existing reviewed bounded stats capture and authored App resources."""
import hashlib
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
REFERENCE = HERE.parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, str(path))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def resources_at(path):
    module("pickup_app_resources", REFERENCE / "client/player_statistics/support.py").resources_at(path)


def run(command, out, stem, timeout=180, env=None):
    helper = module("pickup_bounded_capture", REFERENCE / "client/statistics/support.py")
    result = helper.run_bounded(command, out, stem, timeout=timeout, env=env)
    require(result.returncode == 0 and result.stderr == b"", stem + " failed or wrote stderr")
    require(result.capture_report.get("reader_errors") == {}, stem + " reader errors")
    return result
