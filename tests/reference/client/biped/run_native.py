"""Authored Biped CPU contracts and complete records; no private answers or JAR."""
import argparse
import json
import os
from pathlib import Path
import tempfile
from support import capture, fingerprint, read_native, require, source_binding


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-probe", required=True, type=Path)
    parser.add_argument("--fp-probe", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="biped-native-", dir=args.work_dir.resolve()))
    bindings = source_binding()
    probes = {"native": args.native_probe.resolve(strict=True), "fp": args.fp_probe.resolve(strict=True)}
    before = {str(p): fingerprint(p) for p in probes.values()}
    report = {"status": "RUNNING", "scope": "normal zero-profile CPU construction/identity/failure/retirement",
              "source_binding": bindings, "native_inputs_before": before, "profiles": {}}
    try:
        for mode in ["contracts", "cold", "pair", "pool", "notice", "item", "biped"]:
            process = capture([probes["fp" if mode in ["item", "biped"] else "native"], mode],
                              out, mode, environment=os.environ.copy())
            rows = read_native(out, process, mode)
            report["profiles"][mode] = {"process": process, "records": len(rows)}
        require(sum(report["profiles"][mode]["records"] for mode in ["cold", "pair", "pool", "notice"]) == 838,
                "complete native constructor record inventory")
        after = {str(p): fingerprint(p) for p in probes.values()}
        require(after == before and source_binding() == bindings, "source/executable readback")
        report.update(status="PASS", native_inputs_after=after)
    except Exception as error:
        report.update(status="FAIL", error=type(error).__name__ + ": " + str(error))
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "work_directory": str(out)}))
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
