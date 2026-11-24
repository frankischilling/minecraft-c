"""Capture two unchanged-original CPU Leaf metadata-color and no-op shape runs."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUPPORT = HERE.parent / "controller"
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
spec = importlib.util.spec_from_file_location("harvest_original_libraries", ROOT / "tests/reference/block/harvest/capture_reference.py")
harvest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harvest)
LIBRARIES = harvest.LIBRARIES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    ensure(digest(jar) == JAR, "unchanged supplied client archive")
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
           "pinned original-compatible libraries")
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ("java", "javac")]
    ensure(java.parent == javac.parent, "one observation JDK")
    vm = java.parent / "server/jvm.dll"
    if not vm.is_file():
        vm = java.parent.parent / "lib/server/libjvm.so"
    inputs = [jar, *libraries, java, javac, java.parent.parent / "lib/modules", vm.resolve(strict=True),
              HERE / "LeafInventoryObservation.java", Path(__file__).resolve(),
              ROOT / "tests/reference/block/harvest/capture_reference.py", SUPPORT / "capture.py",
              SUPPORT / "classfile.py"]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    products = output / "classes"
    products.mkdir()
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "inputs_before": before, "runs": []}
    try:
        pins = {"client_jar_sha256": JAR, "classes": {}}
        with zipfile.ZipFile(jar) as archive:
            for name in ("bk", "nr", "uu", "jh"):
                detail = inspect_class(archive.read(name + ".class"))
                detail["declared_methods_complete"] = name != "uu"
                if name == "uu":
                    detail["methods"] = [m for m in detail["methods"]
                                         if m["name"] == "g" and m["descriptor"] == "()V"]
                pins["classes"][name] = detail
        for name in ("bk", "nr"):
            ensure(not any(m["name"] == "g" and m["descriptor"] == "()V"
                           for m in pins["classes"][name]["methods"]), "complete inherited g inventory")
        (output / "source-pins.json").write_text(json.dumps(pins, indent=2) + "\n", encoding="utf-8", newline="\n")
        classpath = os.pathsep.join(str(path) for path in [products, jar, *libraries])
        compiled = capture([str(javac), "-cp", classpath, "-d", str(products),
                            str(HERE / "LeafInventoryObservation.java")], output, "compile", 60)
        require_success(compiled, empty_stderr=True)
        ensure(all(path.name.startswith("LeafInventoryObservation") for path in products.iterdir()),
               "only project observer classes, no original replacements")
        selected = []
        for repeat in range(2):
            observed = capture([str(java), "-Xint", "-Xverify:all", "-Djava.awt.headless=true",
                                "-Djava.security.manager=allow", "-cp", classpath,
                                "LeafInventoryObservation"], output, "original-" + str(repeat), 120)
            report["runs"].append(observed)
            require_success(observed)
            lines = (output / ("original-" + str(repeat) + ".stdout")).read_text().splitlines()
            rows = [line for line in lines if line.startswith(("LEAF_INVENTORY_COLOR ", "LEAF_INVENTORY_SHAPE "))]
            ensure(len(rows) == 17 and [int(line.split()[1]) for line in rows[:16]] == list(range(16)),
                   "complete metadata and shape inventory")
            ensure("LEAF_INVENTORY_GUARD 0 0 65536" in lines, "source guard and actual JH array")
            selected.append(rows)
        ensure(selected[0] == selected[1], "two fresh unchanged-original CPU runs agree")
        (output / "observations.txt").write_text("\n".join(selected[0]) + "\n", encoding="utf-8", newline="\n")
        report.update(status="PASS", rows=17, repeats=2)
    finally:
        report["inputs_after"] = {str(path): digest(path) for path in inputs}
        ensure(report["inputs_after"] == before, "observation inputs unchanged")
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("PASS: two fresh original Leaf inventory CPU captures; " + str(output))


if __name__ == "__main__":
    main()
