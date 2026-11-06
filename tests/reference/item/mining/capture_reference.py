"""Capture actual client Item mining dispatch without modifying game classes."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tests/reference/client/controller"))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = "af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c"
CLASSES = ("gm wc au ta ye yw qz rx qd la rk wa ym vc hu bc rl sa ie dm vd lv mo cs km lp "
           "bl tr bz qk wr ck bi go en mr uy qr dq cp iz ix bu uu ln").split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jar", type=Path, required=True)
    parser.add_argument("--libraries", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--destroy", action="store_true")
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    ensure(digest(jar) == JAR, "verified unchanged client archive")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    products = output / "classes"
    products.mkdir()
    libraries = [args.libraries / name for name in (
        "org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar",
        "org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar",
        "net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar",
        "net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar")]
    libraries = [path.resolve(strict=True) for path in libraries]
    java, javac = (Path(shutil.which(tool)).resolve(strict=True) for tool in ("java", "javac"))
    ensure(java.parent == javac.parent, "one observation JDK")
    observer = "MiningDestroyObservation" if args.destroy else "MiningObservation"
    inputs = [jar, *libraries, java, javac, java.parent.parent / "lib/modules",
              HERE / (observer + ".java"), Path(__file__).resolve(),
              ROOT / "tests/reference/client/controller/capture.py",
              ROOT / "tests/reference/client/controller/classfile.py"]
    vm = java.parent / "server/jvm.dll"
    if not vm.is_file():
        vm = java.parent.parent / "lib/server/libjvm.so"
    inputs.append(vm.resolve(strict=True))
    before = {str(path): digest(path) for path in inputs}
    import os
    classpath = os.pathsep.join(str(path) for path in [products, jar, *libraries])
    pins = {"client_jar_sha256": JAR, "classes": {}}
    with zipfile.ZipFile(jar) as archive:
        for name in CLASSES + (["os", "ob", "gs", "dc", "xi"] if args.destroy else []):
            pins["classes"][name] = inspect_class(archive.read(name + ".class"))
    (output / "source-pins.json").write_text(json.dumps(pins, indent=2) + "\n", encoding="utf-8")
    compiled = capture([str(javac), "-cp", classpath, "-d", str(products),
                        str(HERE / (observer + ".java"))], output, "compile", 60)
    require_success(compiled, empty_stderr=True)
    command = [str(java), "-Xint", "-Xverify:all", "-Xmx256m", "-Djava.awt.headless=true"]
    if args.destroy:
        command.append("-Djava.security.manager=allow")
    command.extend(["-cp", classpath, observer])
    if args.destroy:
        command.append(str(output / "game"))
    observed = capture(command, output, "original", 120)
    require_success(observed, empty_stderr=not args.destroy)
    lines = (output / "original.stdout").read_text(encoding="utf-8").splitlines()
    if args.destroy:
        ensure(sum(line.startswith(("DESTROY\t", "STACK_DESTROY\t")) for line in lines) == 2331,
               "complete original Item/Stack destruction matrix")
        ensure(sum(line.startswith("CONTROLLER_DESTROY\t") for line in lines) == 6,
               "complete original controller destruction matrix")
    else:
        declared = [line.split("\t") for line in lines if line.startswith("CLASS\t")]
        ensure(len(declared) == 202 and len({row[2] for row in declared}) == 38,
               "all actual Item IDs and runtime Classes observed")
        ensure({row[3] for row in declared} == {"gm", "dq", "qd", "bl"}, "exact speed owners")
        ensure({row[4] for row in declared} == {"gm", "wc", "au", "qd", "bl"}, "exact harvest owners")
        ensure(lines[-1] == "TOTAL\t202\t19594", "complete original mining matrix")
    after = {str(path): digest(path) for path in inputs}
    ensure(before == after, "all supplied inputs unchanged")
    scope = ("Actual Item/Stack destruction across all202 Items, durability/count boundaries, "
             "used/broken statistics and six actual controller Stone removals. No graphics/native "
             "library/network/server/full gameplay claim." if args.destroy else
             "Actual Item, Stack and Inventory speed/harvest on every registered Item/Block pair "
             "and NULL Block. Inventory Player is NULL; count=-3, damage=-7. No World, graphics, "
             "native library, server or complete gameplay claim.")
    report = {"status": "PASS", "scope": scope,
              "inputs_before": before, "inputs_after": after,
              "output_sha256": digest(output / "original.stdout")}
    (output / "capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Original mining capture:", output)


if __name__ == "__main__":
    main()
