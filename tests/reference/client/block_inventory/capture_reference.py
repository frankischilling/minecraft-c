"""Capture actual inventory virtuals on all 96 unchanged-original Blocks twice."""
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
spec = importlib.util.spec_from_file_location("inventory_original_libraries", ROOT / "tests/reference/block/harvest/capture_reference.py")
harvest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harvest)
LIBRARIES = harvest.LIBRARIES
TAGS = ("DECL\t", "BLOCK\t", "COLOR\t", "SHAPE\t", "TEXTURE\t", "TEXTURE_LATE\t")


def hierarchy(data):
    cursor = 8

    def take(count):
        nonlocal cursor
        value = data[cursor:cursor + count]
        if len(value) != count:
            raise ValueError("truncated original constant pool")
        cursor += count
        return value

    def u2():
        return int.from_bytes(take(2), "big")

    count = u2()
    pool = [None] * count
    index = 1
    while index < count:
        tag = take(1)[0]
        if tag == 1:
            pool[index] = take(u2()).decode("utf-8", errors="replace")
        elif tag == 7:
            pool[index] = u2()
        elif tag in (3, 4, 9, 10, 11, 12, 17, 18):
            take(4)
        elif tag in (5, 6):
            take(8)
            index += 1
        elif tag in (8, 16, 19, 20):
            take(2)
        elif tag == 15:
            take(3)
        else:
            raise ValueError("original constant pool tag")
        index += 1
    u2()
    actual, parent = u2(), u2()
    return pool[pool[actual]], None if parent == 0 else pool[pool[parent]]


def source_pins(jar):
    with zipfile.ZipFile(jar) as archive:
        classes = {}
        for path in archive.namelist():
            if path.endswith(".class"):
                data = archive.read(path)
                name, parent = hierarchy(data)
                classes[name] = (parent, data)

    def is_block(name):
        seen = set()
        while name in classes and name not in seen:
            if name == "uu":
                return True
            seen.add(name)
            name = classes[name][0]
        return False

    blocks = sorted(name for name in classes if is_block(name))
    ensure(len(blocks) == 73, "complete original Block class hierarchy")
    result = {"client_jar_sha256": JAR, "block_classes": blocks, "classes": {}}
    for name in [*blocks, "jh"]:
        parent, data = classes[name]
        detail = inspect_class(data)
        detail.update(superclass=parent, declared_methods_complete=True)
        result["classes"][name] = detail
    color = [name for name in blocks if any(m["name"] == "b" and m["descriptor"] == "(I)I"
                                            for m in result["classes"][name]["methods"])]
    shape = [name for name in blocks if any(m["name"] == "g" and m["descriptor"] == "()V"
                                            for m in result["classes"][name]["methods"])]
    bounds = [name for name in blocks if any(m["name"] == "a" and m["descriptor"] == "(FFFFFF)V"
                                             for m in result["classes"][name]["methods"])]
    ensure(color == ["bk", "uu"] and shape == ["bv", "jq", "oi", "oq", "um", "uu"] and bounds == ["uu"],
           "complete inventory virtual declarations")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    ensure(digest(jar) == JAR, "unchanged supplied client archive")
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())), "pinned original libraries")
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ("java", "javac")]
    ensure(java.parent == javac.parent, "one observation JDK")
    vm = java.parent / "server/jvm.dll"
    if not vm.is_file():
        vm = java.parent.parent / "lib/server/libjvm.so"
    inputs = [jar, *libraries, java, javac, java.parent.parent / "lib/modules", vm.resolve(strict=True),
              HERE / "BlockInventoryObservation.java", HERE / "inputs.json", Path(__file__).resolve(),
              ROOT / "tests/reference/block/harvest/capture_reference.py", SUPPORT / "capture.py", SUPPORT / "classfile.py"]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    products = output / "classes"
    products.mkdir()
    before = {str(path): digest(path) for path in inputs}
    report = {"status": "FAIL", "inputs_before": before, "runs": []}
    try:
        pins = source_pins(jar)
        (output / "source-pins.json").write_text(json.dumps(pins, indent=2) + "\n", encoding="utf-8", newline="\n")
        classpath = os.pathsep.join(str(path) for path in [products, jar, *libraries])
        compiled = capture([str(javac), "-cp", classpath, "-d", str(products), str(HERE / "BlockInventoryObservation.java")], output, "compile", 60)
        require_success(compiled, empty_stderr=True)
        ensure(all(path.name.startswith("BlockInventoryObservation") for path in products.iterdir()), "only project observer products")
        selections = []
        guards = []
        for repeat in range(2):
            observed = capture([str(java), "-Xint", "-Xverify:all", "-Djava.awt.headless=true",
                                "-Djava.security.manager=allow", "-cp", classpath, "BlockInventoryObservation"],
                               output, "original-" + str(repeat), 180)
            report["runs"].append(observed)
            require_success(observed)
            lines = (output / ("original-" + str(repeat) + ".stdout")).read_text().splitlines()
            rows = [line for line in lines if line.startswith(TAGS)]
            guard = [line for line in lines if line.startswith("BLOCK_INVENTORY_GUARD\t")]
            ensure(len(guard) == 1 and guard[0].split("\t")[1] == "96" and guard[0].split("\t")[-2:] == ["0", "0"], "source guards")
            selections.append(rows)
            guards.append(guard[0])
        ensure(selections[0] == selections[1] and guards[0] == guards[1], "two fresh original captures agree")
        (output / "observations.tsv").write_text("\n".join(selections[0]) + "\n", encoding="ascii", newline="\n")
        (output / "guards.tsv").write_text(guards[0] + "\n", encoding="ascii", newline="\n")
        report.update(status="PASS", rows=len(selections[0]), repeats=2, guard=guards[0])
    finally:
        report["inputs_after"] = {str(path): digest(path) for path in inputs}
        ensure(report["inputs_after"] == before, "original inputs unchanged")
        (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("PASS: two fresh original inventory-family CPU captures; " + str(output))


if __name__ == "__main__":
    main()
