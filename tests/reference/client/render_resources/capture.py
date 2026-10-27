"""Use the existing bounded pipe owner; admit current source and direct products."""
import importlib.util
import json
from pathlib import Path
from protocol import completed, cpu_rows, fingerprint, font_rows, require

HERE = Path(__file__).resolve().parent


def write_json(path, value):
    with Path(path).open("x", encoding="utf-8", newline="\n") as stream:
        text = json.dumps(value, indent=2) + "\n"
        require(stream.write(text) == len(text), "complete receipt write")


def source_inventory(root):
    root = root.resolve(strict=True)
    proof = json.loads((HERE / "provenance.json").read_text(encoding="utf-8"))
    files = proof["source_files"]
    require(type(files) is dict and files, "declared source closure")
    observed = {}
    for relative, expected in files.items():
        path = (root / relative).resolve(strict=True)
        require(path.is_relative_to(root), "source path containment")
        observed[relative] = fingerprint(path)
        require(observed[relative] == expected, "qualified source preimage: " + relative)
    return observed


def owner(root):
    source = root / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("resource_bounded_capture", source)
    require(spec is not None and spec.loader is not None, "bounded capture owner")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.run_bounded


def native(executable, arguments, work, root, mode):
    executable = executable.resolve(strict=True)
    before = source_inventory(root)
    controls = {str(HERE / "provenance.json"): fingerprint(HERE / "provenance.json")}
    binary = fingerprint(executable)
    command = [str(executable), *map(str, arguments)]
    result = owner(root)(command, work, "native", timeout=180)
    completed(result.capture_report, result.stdout, result.stderr, executable)
    require(result.capture_report["command"] == command, "exact native arguments")
    parsed = cpu_rows(result.stdout) if mode == "cpu" else font_rows(result.stdout)
    require(source_inventory(root) == before, "source unchanged through real child")
    require(fingerprint(executable) == binary, "native executable unchanged")
    require(all(fingerprint(Path(p)) == pin for p, pin in controls.items()), "admission metadata unchanged")
    products = {name: fingerprint(work / name) for name in
                ("native.capture.json", "native.stdout", "native.stderr")}
    report = {"status": "PASS", "mode": mode, "executable": str(executable),
              "executable_before": binary, "executable_after": fingerprint(executable),
              "source_before": before, "source_after": before, "products": products,
              "capture": result.capture_report, "rows": len(parsed) if mode == "cpu" else parsed["rows"]}
    if mode == "font":
        report["checks"] = parsed["checks"]
        report["device"] = parsed["device"]
    write_json(work / "native-run.json", report)
    return result, report
