"""Run real native SDK owners with authored imported-file inputs and strict capture."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = ROOT / "tests/reference/client/display_sdk"
sys.path.insert(0, str(HERE))
from protocol import footer, require


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, "actual capture module")
    owner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(owner)
    return owner


def authored_resources(root):
    root.mkdir()
    (root / "sdk").mkdir()
    (root / "sdk/lwjgl-icon-16.rgba").write_bytes(bytes((17, 53, 101, 255)) * 16 * 16)
    (root / "sdk/lwjgl-icon-32.rgba").write_bytes(bytes((211, 137, 29, 127)) * 32 * 32)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--kind", choices=("queue", "icons", "transport", "window"), required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="sdk-" + args.kind + "-", dir=args.work_dir.resolve()))
    shared = module(ROOT / "tests/reference/client/render_resources/protocol.py", "sdk_shared_completion")
    capture = module(ROOT / "tests/reference/client/statistics/support.py", "sdk_shared_capture")
    controls = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
    source = {}
    for relative, expected in controls["source_files"].items():
        path = (ROOT / relative).resolve(strict=True)
        require(path.is_relative_to(ROOT), "source path containment")
        source[relative] = shared.fingerprint(path)
        require(source[relative] == expected, "current declared source: " + relative)
    executable = args.executable.resolve(strict=True)
    binary = shared.fingerprint(executable)
    metadata = shared.fingerprint(HERE / "provenance.json")
    resources = work / "resources"
    arguments = []
    files = {}
    if args.kind in ("icons", "window"):
        authored_resources(resources)
        arguments = [str(resources)]
        files = {p.relative_to(resources).as_posix(): shared.fingerprint(p) for p in resources.rglob("*") if p.is_file()}
    command = [str(executable), *arguments]
    result = capture.run_bounded(command, work, "native", timeout=180)
    try:
        shared.completed(result.capture_report, result.stdout, result.stderr, executable)
    except ValueError as error:
        raise ValueError(
            f"{error}\nnative stdout tail: {result.stdout[-8192:]!r}"
            f"\nnative stderr tail: {result.stderr[-8192:]!r}"
        ) from error
    require(result.capture_report["command"] == command, "exact direct native arguments")
    require(json.loads((work / "native.capture.json").read_text()) == result.capture_report, "actual complete capture receipt")
    require((work / "native.stdout").read_bytes() == result.stdout and
            (work / "native.stderr").read_bytes() == result.stderr, "actual retained streams")
    parsed = footer(result.stdout, args.kind)
    require(shared.fingerprint(executable) == binary, "actual executable unchanged")
    require(shared.fingerprint(HERE / "provenance.json") == metadata, "native source authority unchanged")
    require(all(shared.fingerprint(ROOT / p) == expected for p, expected in source.items()), "actual source unchanged")
    after_files = {} if not files else {p.relative_to(resources).as_posix(): shared.fingerprint(p)
                                       for p in resources.rglob("*") if p.is_file()}
    require(after_files == files, "exact authored real file inventory unchanged")
    products = {p: shared.fingerprint(work / p) for p in ("native.capture.json", "native.stdout", "native.stderr")}
    report = {"status": "PASS", "scope": "authored real native owners; no original or physical input-record parity",
              "executable": str(executable), "executable_before": binary, "executable_after": binary,
              "source_before": source, "source_after": source, "capture": result.capture_report,
              "native": parsed, "resources_before": files, "resources_after": files, "products": products}
    with (work / "report.json").open("x", encoding="utf-8", newline="\n") as target:
        body = json.dumps(report, indent=2) + "\n"
        require(target.write(body) == len(body), "complete native report write")
    print(work)


if __name__ == "__main__":
    main()
