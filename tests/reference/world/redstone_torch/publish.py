"""Publish only a separately admitted fresh public capture; never run the game."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture",type=Path,required=True)
    args=parser.parse_args()
    spec=importlib.util.spec_from_file_location("torch_publish_packet",HERE/"packet.py")
    packet=importlib.util.module_from_spec(spec);spec.loader.exec_module(packet)
    packet.need(packet.MANIFEST_SHA256=="UNFROZEN" and not (HERE/"manifest.json").exists(),
                "publication is one-shot; retain predecessors separately")
    source=args.capture.resolve(strict=True)
    summary=packet.admit_capture(source,HERE)
    names=["capture.json","freeze.json","jdk-code-pins.json"]
    names += [stem+"."+suffix for stem in ("java","javac","compile")
              for suffix in ("stdout","stderr","capture.json")]
    names += [cohort+"-ordinary-"+str(repeat)+"/"+name
              for cohort in packet.COHORTS for repeat in (0,1)
              for name in ("original.stdout","original.stderr","original.capture.json",
                           "selected.txt","own-entropy.jsonl","source-exceptions.jsonl")]
    packet.need(all(not (HERE/name).exists() for name in names),"no capture artifact may be overwritten")
    for name in names:
        destination=HERE/name;destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source/name,destination)
    # No compiled helper, World save output, JAR, class, jmod or DLL is published.
    manifest=dict(format=1,client_jar_sha256=packet.JAR,
        packet_body_sha256=hashlib.sha256(packet.unbound_packet((HERE/"packet.py").read_bytes())).hexdigest(),files_sha256={
        p.relative_to(HERE).as_posix():packet.sha(p) for p in sorted(HERE.rglob("*"))
        if p.is_file() and "__pycache__" not in p.parts and
        p.name not in ("packet.py","manifest.json")})
    target=HERE/"manifest.json"
    target.write_text(json.dumps(manifest,indent=2)+"\n",encoding="ascii",newline="\n")
    raw=(HERE/"packet.py").read_bytes()
    bound=packet.unbound_packet(raw).replace(b"MANIFEST_SHA256 = 'UNFROZEN'",
        ("MANIFEST_SHA256 = '"+packet.sha(target)+"'").encode(),1)
    (HERE/"packet.py").write_bytes(bound)
    packet.MANIFEST_SHA256=packet.sha(target)
    admitted=packet.audit()
    print(admitted["status"],"406 fresh Source rows; full_entropy_qualification=false")


if __name__=="__main__":main()
