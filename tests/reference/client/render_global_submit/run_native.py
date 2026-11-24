"""Replay genuine-device N profiles with explicit optional-capability scope."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import rows
from capture import capture, digest, ensure, require_success


def capability_receipt(text, name, value):
    """Require a genuine completed CX query before any N graph setup/rows."""
    lines = text.splitlines()

    def one(prefix):
        found = [line for line in lines if line.startswith(prefix)]
        ensure(len(found) == 1, "single actual " + prefix + "receipt")
        return found[0]

    format_line = one("SUBMIT_FORMAT ")
    ensure(re.fullmatch(r"SUBMIT_FORMAT 8 8 8 8 [1-9][0-9]* [0-9]+", format_line),
           "actual RGBA8/depth device format")
    context_line = one("SUBMIT_CONTEXT ")
    match = re.fullmatch(r"SUBMIT_CONTEXT " + re.escape(name) + r" ([0-9]+) ([0-9]+) ([0-9]+)", context_line)
    ensure(match is not None, "actual Display context receipt")
    major, minor, profile = map(int, match.groups())
    ensure(major >= 3 and profile & 1, "actual admitted core Display context")
    strings = {}
    string_lines = []
    for key in ("VENDOR", "RENDERER", "VERSION"):
        line = one("SUBMIT_" + key + " ")
        prefix = "SUBMIT_" + key + " " + name + " "
        ensure(line.startswith(prefix) and line[len(prefix):].strip(), "actual Display " + key)
        strings[key.lower()] = line[len(prefix):]
        string_lines.append(line)
    cx_line = one("SUBMIT_CX ")
    ensure(cx_line == ("SUBMIT_CX " + name + " status=0 static_a=1 source_returned=1 capability_read=1 "
                       "capability=" + str(value) + " source_pc=20"),
           "successful actual CX static/query/source-return evidence")
    capability_line = one("SUBMIT_CAPABILITY ")
    ensure(capability_line == "SUBMIT_CAPABILITY " + name + " " + str(value), "actual CX capability")
    headers = [format_line, context_line, *string_lines, cx_line, capability_line]
    ensure(lines[:len(headers)] == headers, "capability admission precedes N setup/rows")
    return {"gl_major": major, "gl_minor": minor, "profile": profile, **strings,
            "cx": cx_line, "capability": value}, headers


def unsupported_receipt(receipt, text, case):
    ensure(case["name"] == "occlusion16" and case.get("actual_capability") is True,
           "exit77 is only an actual-capability occlusion16 result")
    ensure(type(receipt["exit"]) is int and receipt["exit"] == 77, "explicit unsupported process result")
    ensure(receipt.get("reader_errors") == {} and receipt["timed_out"] is False and
           receipt["output_overflow"] is False and receipt["pipe_capture_complete"] is True and
           receipt["stderr_bytes"] == 0, "complete unsupported capture with empty stderr")
    context, headers = capability_receipt(text, case["name"], 0)
    marker = "Source N submit unsupported: occlusion16; N-not-entered; cleanup-complete"
    ensure(text.splitlines() == [*headers, marker],
           "unsupported context must have no SUBMIT/TG rows or N normal return and complete teardown")
    return context


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--require-occlusion", action="store_true",
                        help="require the actual supported-context occlusion16 profile and all35 rows")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="terrain-submit-", dir=args.work_dir.resolve()))
    cases = json.loads((HERE / "inputs.json").read_text())["cases"]
    expected = json.loads((HERE / "finite.json").read_text())["rows"]
    ensure(len(cases) == 7 and len(expected) == 35, "complete original device cohort")
    inputs = [executable, HERE / "finite.json", HERE / "inputs.json", HERE / "source-pins.json",
              HERE / "SubmitObservation.java", HERE / "capture_reference.py", HERE / "run_native.py",
              HERE.parent / "controller/capture.py", HERE.parent / "controller/classfile.py",
              HERE.parent / "render_global_sky/capture_prefix.py"]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {"status": "FAIL", "input_before_sha256": before, "profiles": [],
              "require_occlusion": args.require_occlusion, "original_rows": 35}
    try:
        observed = []
        expected_observed = []
        unsupported = []
        completed = 0
        for i, case in enumerate(cases):
            original = expected[5 * i:5 * (i + 1)]
            ensure(rows("\n".join(original), case["name"]) == original, "original row inventory")
            receipt = capture([str(executable), str(i)], output, "native" + str(i), 30, os.environ.copy(), output.resolve())
            profile = {"name": case["name"], "capture": receipt}
            report["profiles"].append(profile)
            text = (output / receipt["stdout_file"]).read_text()
            if receipt["exit"] == 77:
                profile["context"] = unsupported_receipt(receipt, text, case)
                profile.update(status="UNSUPPORTED", actual_rows=[], uncredited_original_rows=original)
                unsupported.append(case["name"])
                continue
            require_success(receipt, empty_stderr=True)
            ensure(re.fullmatch(r"Source N submit: " + re.escape(case["name"]) + r"; [1-9][0-9]* checks", text.splitlines()[-1]),
                   "normal native completion")
            actual = rows(text, case["name"])
            profile["actual_rows"] = actual
            profile["format"] = [line for line in text.splitlines() if line.startswith("SUBMIT_FORMAT ")]
            capability = [line for line in text.splitlines() if line.startswith("SUBMIT_CAPABILITY ")]
            profile["capability"] = capability
            ensure(capability == (["SUBMIT_CAPABILITY " + case["name"] + " 1"] if case.get("actual_capability") else []),
                   "actual supported-context occlusion capability differs; no forced branch or visibility substitute")
            if case.get("actual_capability"):
                profile["context"], _ = capability_receipt(text, case["name"], 1)
            ensure(not any(line.startswith("Source N submit unsupported:") for line in text.splitlines()),
                   "successful profile must not claim unsupported completion")
            profile["mismatches"] = [{"index": j, "native": a, "original": b}
                                      for j, (a, b) in enumerate(zip(actual, original)) if a != b]
            ensure(actual == original, "native/original complete device rows differ")
            observed += actual
            expected_observed += original
            completed += 1
            profile["status"] = "PASS"
        ensure(observed == expected_observed, "every completed original row including exact pixels")
        if unsupported:
            ensure(unsupported == ["occlusion16"] and completed == 6 and len(observed) == 30 and
                   observed == expected[:30], "exact30 ordinary rows; five occlusion rows remain uncredited")
        else:
            ensure(completed == 7 and observed == expected, "all35 original rows including exact pixels")
        report.update(rows=len(observed), compared_rows=len(observed), profiles_completed=completed,
                      unsupported_profiles=unsupported, uncredited_rows=35 - len(observed))
        ensure(not (args.require_occlusion and unsupported),
               "--require-occlusion requires genuine supported CX and all35 rows; occlusion16 is unsupported")
        report["status"] = "PASS_WITH_UNSUPPORTED" if unsupported else "PASS"
    finally:
        report["input_after_sha256"] = {str(p.resolve()): digest(p) for p in inputs}
        same = before == report["input_after_sha256"]
        if not same:
            report["status"] = "FAIL"
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        ensure(same, "native executable and fixture inputs unchanged")
    print("Source N submit: " + report["status"] + "; compared_rows=" + str(report["compared_rows"]) +
          "/original_rows=35; completed=" + str(report["profiles_completed"]) + "; unsupported=" +
          (",".join(report["unsupported_profiles"]) or "-") + "; uncredited_rows=" +
          str(report["uncredited_rows"]) + "; " + str(output / "run.json"))


if __name__ == "__main__":
    main()
