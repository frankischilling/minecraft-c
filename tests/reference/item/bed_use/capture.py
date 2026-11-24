"""Capture actual LP.a in two fresh bounded VMs with read-only JDI receipts."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import packet


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C = module("bed_placement_capture", ROOT / "tests/reference/client/controller/capture.py")
P = module("bed_placement_world", ROOT / "tests/reference/client/picking/observe.py")
AUTHORED = ("BedPlacementObservation.java", "BedPlacementJdi.java", "prepare_inputs.py", "capture.py",
            "packet.py", "profiles.tsv", "finite.json", "source-pins.json")


def inventory(path):
    return {p.relative_to(path).as_posix(): packet.digest(p) for p in sorted(path.rglob("*")) if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def private_output(path):
    out = Path(path).resolve()
    packet.need(out.is_relative_to(ROOT / ".local") and out != ROOT / ".local",
                "all raw attempt outputs must remain under workspace .local")
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = private_output(args.output)
    out.mkdir(parents=True, exist_ok=False)
    classes = out / "classes"
    classes.mkdir()
    jar = ROOT / "b1.7.3.jar"
    pins = packet.source_pins(jar)
    profiles = packet.profiles()
    java, javac = [Path(shutil.which(n)).resolve(strict=True) for n in ("java", "javac")]
    jdk = java.parent.parent
    libraries = [Path(os.environ["APPDATA"]) / ".minecraft/libraries" / name for name in P.LIBRARIES]
    packet.need(all(packet.digest(p) == h for p, h in zip(libraries, P.LIBRARIES.values())), "pinned original SDK")
    support = [ROOT / n for n in ("tests/reference/client/controller/capture.py", "tests/reference/client/controller/classfile.py",
                "tests/reference/client/picking/observe.py", "tests/tools/client/test_picking.py")]
    inputs = [jar, java, javac, jdk / "lib/modules", *java.parent.glob("*.dll"), jdk / "bin/server/jvm.dll",
              *libraries, *support, *(HERE / n for n in AUTHORED)]
    before = {str(p.resolve(strict=True)): packet.digest(p) for p in sorted(set(inputs))}
    report = {"status":"FAIL", "schema":"PRIVATE_BED_PLACEMENT_CAPTURE_V1", "input_before":before,
              "scope":"Actual direct LP.a placement on genuine saved World/Chunk/Player/Stack/Bed owners. Two fresh read-only JDI target VMs, declared source operands/callbacks, no original game-class substitution, native C jobs, GUI, wrapper statistics or full sleep claim.", "runs":[]}
    frozen = out / "frozen"
    frozen.mkdir()
    for name in AUTHORED:
        shutil.copy2(HERE / name, frozen / name)
    environment = os.environ.copy()
    for n in ("JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS"):
        environment.pop(n, None)
    try:
        for role, tool in (("java",java),("javac",javac)):
            receipt = C.capture([str(tool),"-version"],out,role,10,environment)
            C.require_success(receipt)
            report[role] = receipt
            version = (out / receipt["stdout_file"]).read_text() + (out / receipt["stderr_file"]).read_text()
            packet.need("17.0.17" in version,"qualified JDK17 version")
        cp = os.pathsep.join(map(str,[classes,jar,*libraries]))
        receipt = C.capture([str(javac),"-encoding","UTF-8","--add-modules","jdk.jdi","-cp",cp,
                    "-d",str(classes),str(HERE/"BedPlacementObservation.java"),str(HERE/"BedPlacementJdi.java")],out,"compile",60,environment)
        report["compile"] = receipt
        C.require_success(receipt,empty_stderr=True)
        helpers = inventory(classes)
        packet.need(set(helpers) == {"BedPlacementObservation.class","BedPlacementObservation$Observer.class",
                    "BedPlacementObservation$DeclaredCallbackFailure.class","BedPlacementObservation$1.class","BedPlacementJdi.class"},"authored observer classes only")
        report["helpers_before"] = helpers
        selected_sets, event_sets = [], []
        for index in range(2):
            run = out / ("original-"+str(index))
            run.mkdir()
            world = run / "world"
            P.world(world)
            world_before = inventory(world)
            receipt = C.capture([str(java),"--add-modules","jdk.jdi","-cp",str(classes),"BedPlacementJdi",
                        str(run),str(jdk),cp,str(jar),str(world),str(HERE/"profiles.tsv")],run,"controller",175,environment,run)
            entry = {"directory":run.name,"receipt":receipt,"world_before":world_before,"world_after":inventory(world)}
            report["runs"].append(entry)
            C.require_success(receipt,empty_stderr=True)
            stdout = (run / "target.stdout").read_text(encoding="ascii")
            selected, records = packet.rows(stdout)
            lines = stdout.splitlines()
            packet.need(lines[-2:] == ["BED_PLACEMENT_GUARD network=0 native=0 exec=0 denied-writes=0 canonical-chunks=4 origin-classes=15 source-class-replacements=0",
                        "BED_PLACEMENT_COMPLETE rows="+str(len(profiles))+" guard-controls=5 jdi-source-writes=0 jdi-target-invocations=0"],"original completion and tested effect guards")
            journal = (run / "controller.stdout").read_text(encoding="ascii").splitlines()
            packet.need(journal[-1].startswith("BED_PLACEMENT_JDI_COMPLETE ") and journal[-1].endswith(" target-exit=0 source-writes=0 target-invocations=0"),"passive JDI terminal receipt")
            live = [line for line in journal if line.startswith("BED_PLACEMENT_LIVE_CODE\t")]
            for line in live:
                _, owner, name, descriptor, length, digest = line.split("\t")
                methods = [m for m in pins["classes"][owner]["methods"] if m["name"]==name and m["descriptor"]==descriptor]
                packet.need(len(methods)==1 and methods[0]["code_bytes"]==int(length) and methods[0]["code_sha256"]==digest,"live reached original bytecodes unchanged")
            events = [json.loads(line) for line in packet.read_lines(run/"events.jsonl")]
            packet.check_events(events,records)
            packet.need(all(name=="session.lock" or entry["world_after"].get(name)==h for name,h in world_before.items()),"initial saved World inputs unchanged except source session lock")
            entry.update(rows=len(records),events=len(events),live_methods=len(live),jdi_completion=journal[-1],
                         artifacts={n:packet.digest(run/n) for n in ("target.stdout","target.stderr","events.jsonl","controller.stdout","controller.stderr","controller.capture.json")})
            selected_sets.append(selected)
            event_sets.append(events)
        packet.need(selected_sets[0]==selected_sets[1],"both unchanged original row sets agree")
        packet.need(event_sets[0]==event_sets[1],"both read-only source-order journals agree")
        live_code = []
        for line in live:
            _, owner, name, descriptor, length, sha = line.split("\t")
            live_code.append({"class": owner, "method": name, "descriptor": descriptor,
                              "bytes": int(length), "sha256": sha})
        canonical = packet.canonical_journal(event_sets[0], live_code)
        packet.need(packet.journal_events(canonical) == event_sets[0], "lossless canonical selected trace")
        write(out / "source-journal.json", canonical)
        (out/"selected.txt").write_text("\n".join(selected_sets[0])+"\n",encoding="ascii",newline="\n")
        report.update(status="PASS",original_fresh_vms=2,rows_per_vm=len(profiles),events_per_vm=len(event_sets[0]),
                      selected_sha256=packet.digest(out/"selected.txt"),journals_equal=True)
    except BaseException as e:
        report["error"]={"type":type(e).__name__,"message":str(e)}
        raise
    finally:
        report["input_after"]={name:packet.digest(name) for name in before}
        report["input_unchanged"]=report["input_after"]==before
        report["helpers_after"]=inventory(classes)
        report["helpers_unchanged"]=report.get("helpers_before")==report["helpers_after"]
        if not report["input_unchanged"] or not report["helpers_unchanged"]:
            report["status"]="FAIL"
        write(out/"report.json",report)
    packet.verify_capture(out)
    print("PASS private LP placement: "+str(len(profiles))+" rows per VM; "+str(out/"report.json"))


if __name__=="__main__":
    main()
