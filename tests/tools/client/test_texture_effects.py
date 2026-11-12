"""Bounded actual C texture-effect contracts on a fresh authored file tree."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def _admit_output(profile, stdout):
    prefixes = {"cpu": b"Actual texture-effect CPU controls", "registration": b"Actual g registration controls",
                "allocation": b"Actual g allocation controls", "world": b"Actual effect World/Player controls",
                "spawn": b"Actual spawn extraction controls"}
    if profile not in prefixes or not isinstance(stdout, bytes):
        return False
    # Each profile begins with Statistics-first initialization. Registration's
    # later cold Fire fixture also activates a separate Block-first namespace.
    banners = rb"151 recipes\r?\n16 achievements\r?\n"
    if profile == "registration":
        banners += rb"16 achievements\r?\n151 recipes\r?\n"
    pattern = banners + re.escape(prefixes[profile]) + rb": [1-9][0-9]*\r?\n"
    return re.fullmatch(pattern, stdout) is not None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--profile", choices=["cpu", "registration", "allocation", "world", "spawn"], required=True)
    args = parser.parse_args()
    helper = ROOT / "tests/reference/client/statistics/support.py"
    spec = importlib.util.spec_from_file_location("effect_contract_transport", helper)
    require(spec is not None and spec.loader is not None, "actual capture helper")
    support = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = support
    spec.loader.exec_module(support)
    work = support.fresh_directory(args.work_dir, "texture-effects-")
    executable = args.executable.resolve(strict=True)
    names = [str(path.relative_to(ROOT)).replace("\\", "/")
             for base in (ROOT / "src/client/texture_effects", ROOT / "tests/client/texture_effects")
             for path in sorted(base.glob("*")) if path.suffix in [".c", ".h"]]
    names += ["src/client/render_engine/effects.c", "src/client/render_engine/effects.h",
              "src/client/render_engine/owner.c", "src/client/render_engine/internal.h",
              "src/client/images/class_url.c", "src/client/images/class_url.h",
              "src/world/runtime/spawn.c", "src/world/runtime/spawn_internal.h",
              "src/entity/player/construction.c", "src/entity/player/atan2.c",
              "src/resources/files/files.c", "include/mc/resources/files/files.h",
              "include/mc/client/texture_effects/effect.h", "tests/tools/client/test_texture_effects.py",
              "src/item/recipes/managers.c", "src/statistics/achievements/achievements.c",
              "src/block/registry/bootstrap.c", "src/item/registry/registry.c",
              "src/statistics/registry/arrays.c"]
    before = {name: support.fingerprint(ROOT / name) for name in names}
    binary_before, helper_before = support.fingerprint(executable), support.fingerprint(helper)
    command = [str(executable), str(work / "owned")]
    result = support.run_bounded(command, work, "native", timeout=180, limit=4 * 1024 * 1024)
    capture = result.capture_report
    require(type(result.returncode) is int and result.returncode == 0, "actual C success")
    require(type(capture.get("exit")) is int and capture["exit"] == 0, "captured C success")
    require(capture.get("pipe_capture_complete") is True and capture.get("reaped") is True,
            "all streams and process closed")
    require(capture.get("timed_out") is False and capture.get("output_overflow") is False and
            capture.get("reader_errors") == {}, "bounded capture complete")
    require(capture.get("command") == command, "exact argv")
    require(type(capture.get("stdout_bytes")) is int and capture["stdout_bytes"] == len(result.stdout),
            "complete stdout")
    require(type(capture.get("stderr_bytes")) is int and capture["stderr_bytes"] == 0 and
            result.stderr == b"", "empty actual stderr")
    require(_admit_output(args.profile, result.stdout), "complete ordered source banners and control footer")
    require({name: support.fingerprint(ROOT / name) for name in names} == before and
            support.fingerprint(executable) == binary_before and support.fingerprint(helper) == helper_before,
            "source, binary and transport unchanged")
    report = {"status": "PASS_AUTHORED_NATIVE_CONTRACTS", "profile": args.profile,
              "command": command, "source": before, "executable": binary_before, "helper": helper_before,
              "capture": support.fingerprint(work / "native.capture.json"),
              "stdout": support.fingerprint(work / "native.stdout"),
              "stderr": support.fingerprint(work / "native.stderr"),
              "authored_inputs_after": {str(p.relative_to(work / "owned")): support.fingerprint(p)
                                        for p in sorted((work / "owned").rglob("*")) if p.is_file()},
              "original_calls": 0, "jvm_calls": 0}
    (work / "REPORT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"texture effects {args.profile}: PASS; actual files/captures retained in {work}")


if __name__ == "__main__":
    main()
