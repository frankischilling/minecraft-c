"""Compare one fresh native CV.g cohort with its complete original literal rows."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
BOOTSTRAP_SEPARATOR = b"\r\n" if os.name == "nt" else b"\n"
# The native picking fixture initializes Recipes before Achievements. These
# setup messages are outside the entered CV.g comparison and keep their order.
BOOTSTRAP = BOOTSTRAP_SEPARATOR.join((b"151 recipes", b"16 achievements")) + BOOTSTRAP_SEPARATOR
COLD_BOOTSTRAP = BOOTSTRAP_SEPARATOR.join((b"16 achievements", b"151 recipes")) + BOOTSTRAP_SEPARATOR
CALLBACK_ROWS = [
    "REDSTONE_CALLBACK brightness receiver=EW block=Stone alias=NULL captured=1 fresh=1 split-context=1",
    "REDSTONE_CALLBACK brightness receiver=World block=Stone alias=foreign captured=1 fresh=1 split-context=1",
    "REDSTONE_CALLBACK brightness receiver=EW block=Stairs alias=foreign captured=1 fresh=1 split-context=1",
    "REDSTONE_CALLBACK brightness receiver=World block=Stairs alias=NULL captured=1 fresh=1 split-context=1",
]
CALLBACK_COMPLETE = "REDSTONE_CALLBACK_COMPLETE native-only=1 cold-registries=4 cache-retired=1"


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(root.rglob("*")) if p.is_file()}


def profiles_at(path, layout):
    rows = []
    for line in path.read_text(encoding="ascii").splitlines():
        values = line.split()
        if len(values) != 14 or not re.fullmatch(r"[A-Za-z0-9_.-]+", values[0]):
            raise ValueError("Profile requires a name and thirteen signed operands")
        if any(re.fullmatch(r"-?(?:0|[1-9][0-9]*)", v) is None for v in values[1:]):
            raise ValueError("Canonical signed profile integers required")
        operands = list(map(int, values[1:]))
        if any(not -(1 << 31) <= v < (1 << 31) for v in operands):
            raise ValueError("Profile operand outside Java int")
        rows.append((values[0], operands))
    if len(rows) != {"flat": 56, "vertical": 20, "capability": 10, "signed": 8, "nullable": 6}[layout]:
        raise ValueError("The renderer cohorts require separate fresh processes")
    return rows


def reference_at(path, profiles, tag):
    wire = path.read_bytes()
    lines = wire.decode("ascii").splitlines()
    if wire != ("\n".join(lines) + "\n").encode("ascii"):
        raise ValueError("Reference projection must retain exact LF rows")
    if len(lines) != 3 * len(profiles) + 4 or not lines[0].startswith(tag + "_STATIC\t"):
        raise ValueError("Complete original static/input/effect/backing/guard/completion projection")
    for index, (name, operands) in enumerate(profiles):
        inp, row, backing = (line.split("\t") for line in lines[1 + 3 * index:4 + 3 * index])
        expected_width = {"REDSTONE": 12, "VERTICAL": 16, "CAPABILITY": 14,
                          "SIGNED": 16, "NULLABLE": 16}[tag]
        if (len(inp) != expected_width or inp[:4] != [tag + "_INPUT", str(index), "sm", "55"] or
                inp[5] != str(operands[5]) or inp[9:12] != list(map(str, (operands[2], operands[3], operands[7]))) or
                (tag == "VERTICAL" and inp[12:] != list(map(str, operands[8:12]))) or
                (tag == "CAPABILITY" and (inp[12] != str(operands[8]) or not inp[13])) or
                (tag == "SIGNED" and inp[12:15] != list(map(str, operands[8:11]))) or
                (tag == "NULLABLE" and inp[12:15] != list(map(str, (operands[8], operands[9], operands[6])))) or
                len(row) != 16 or row[:4] != [tag, str(index), name, str(operands[0])] or
                len(backing) != 6 or backing[:2] != [tag + "_BACKING", str(index)]):
            raise ValueError(f"Original profile/row identity differs at {index}")
    if (re.fullmatch(re.escape(tag) + r"_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=[0-9]+", lines[-3]) is None or
            lines[-2] != tag + "_GUARD controls network=2 native=1 denied-writes=1 execs=1" or
            not lines[-1].startswith(tag + f"_COMPLETE rows={len(profiles)} ")):
        raise ValueError("Original guard and completion evidence required")
    # SecurityManager denial receipts remain original supporting evidence; the
    # native fixture does not claim to execute that Java guard implementation.
    return lines[:-3] + lines[-1:]


def signed_owner_receipt(wire):
    """Read the literal request-slot/returned-owner projection, without remapping it."""
    line = wire.decode("ascii")
    if not line.endswith("\n") or line.count("\n") != 1 or "\r" in line:
        raise ValueError("Signed owner receipt requires one exact ASCII LF row")
    fields = line[:-1].split("\t")
    if (len(fields) != 15 or fields[:2] != ["SIGNED_OWNERS", "12"] or
            re.fullmatch(r"[1-9][0-9]*", fields[2]) is None):
        raise ValueError("Complete signed owner receipt required")
    slots = []
    for index, field in enumerate(fields[3:]):
        values = field.split(",")
        if (len(values) != 6 or any(re.fullmatch(r"(?:0|-?[1-9][0-9]*)", value) is None
                                    for value in values)):
            raise ValueError("Canonical signed owner operands required")
        slot = list(map(int, values))
        if (slot[0] != index or any(not -(1 << 31) <= value < (1 << 31) for value in slot) or
                not 0 <= slot[5] <= index):
            raise ValueError("Signed request slot or first-equal slot differs")
        slots.append(slot)
        first = slots[slot[5]]
        if first[5] != slot[5] or slot[3:5] != first[3:5]:
            raise ValueError("Signed alias must retain its first returned owner's coordinates")
    if int(fields[2]) != sum(slot[5] == index for index, slot in enumerate(slots)):
        raise ValueError("Signed unique-owner count differs")
    return line[:-1]


def select_native(wire, tag, count, owners=None):
    if not wire.startswith(BOOTSTRAP):
        raise ValueError("Native Registry bootstrap output differs")
    data = wire[len(BOOTSTRAP):]
    if tag == "SIGNED":
        if owners is None:
            raise ValueError("Signed replay requires its separate original owner receipt")
        signed_owner_receipt(owners)
        if not data.startswith(owners):
            raise ValueError("Native signed owner receipt differs from the original literal row")
        data = data[len(owners):]
    elif owners is not None:
        raise ValueError("Only signed replay admits an owner receipt")
    lines = data.decode("ascii").splitlines()
    if data != ("\n".join(lines) + "\n").encode("ascii"):
        raise ValueError("Native data projection must retain exact LF separators")
    if (len(lines) != count * 3 + 3 or
            not native_marker(lines[-1], tag == "REDSTONE") or
            not lines[0].startswith(tag + "_STATIC\t") or
            not lines[-2].startswith(tag + f"_COMPLETE rows={count} ")):
        raise ValueError("Complete native field/backing/cleanup receipt required")
    for index in range(count):
        inp, row, backing = lines[1 + 3 * index:4 + 3 * index]
        if (not inp.startswith(tag + f"_INPUT\t{index}\t") or
                not row.startswith(tag + f"\t{index}\t") or
                not backing.startswith(tag + f"_BACKING\t{index}\t")):
            raise ValueError(f"Native original row order differs at {index}")
    return lines[:-1], lines[-1]


def compare(actual, expected):
    if actual != expected:
        index = next((i for i, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]),
                     min(len(actual), len(expected)))
        raise ValueError(f"Redstone literal mismatch at {index}: native={actual[index:index + 1]!r} "
                         f"original={expected[index:index + 1]!r}")


def native_marker(line, flat):
    match = re.fullmatch(r"REDSTONE_NATIVE checks=([1-9][0-9]*) cleanup=1 dispatcher=([01]) "
                         r"dispatcher-checks=(0|[1-9][0-9]*)", line)
    return (match is not None and int(match[2]) == int(flat) and
            (int(match[3]) > 0 if flat else int(match[3]) == 0) and
            int(match[3]) <= int(match[1]))


def sample_native_marker(flat):
    return ("REDSTONE_NATIVE checks=1 cleanup=1 dispatcher=" + ("1" if flat else "0") +
            " dispatcher-checks=" + ("1" if flat else "0"))


def select_callbacks(wire):
    if not wire.startswith(BOOTSTRAP):
        raise ValueError("Native picking constructor output differs")
    remaining = wire[len(BOOTSTRAP):]
    for row in CALLBACK_ROWS:
        exact = COLD_BOOTSTRAP + row.encode("ascii") + b"\n"
        if not remaining.startswith(exact):
            raise ValueError("Exact cold Registry constructor/callback order differs")
        remaining = remaining[len(exact):]
    lines = remaining.decode("ascii").splitlines()
    if (len(lines) != 2 or remaining != ("\n".join(lines) + "\n").encode("ascii") or
            lines[0] != CALLBACK_COMPLETE or
            not native_marker(lines[1], False)):
        raise ValueError("Native callback and full-owner cleanup receipts required")
    return {"constructor_groups": 5, "callbacks": CALLBACK_ROWS.copy(),
            "complete": lines[0], "native": lines[1]}


def callback_controls():
    wire = BOOTSTRAP + b"".join(COLD_BOOTSTRAP + row.encode("ascii") + b"\n"
                                 for row in CALLBACK_ROWS)
    wire += (CALLBACK_COMPLETE + "\n" + sample_native_marker(False) + "\n").encode("ascii")
    select_callbacks(wire)
    mutations = [wire[len(BOOTSTRAP):], wire[:-1], wire + b"unexpected\n",
                 wire.replace(COLD_BOOTSTRAP, BOOTSTRAP, 1),
                 wire.replace(COLD_BOOTSTRAP, b"", 1),
                 wire.replace(b"captured=1", b"captured=0", 1),
                 wire.replace(b"fresh=1", b"fresh=0", 1),
                 wire.replace(b"split-context=1", b"split-context=0", 1),
                 wire.replace(b"receiver=World", b"receiver=EW", 1),
                 wire.replace(b"cold-registries=4", b"cold-registries=3"),
                 wire.replace(b"cache-retired=1", b"cache-retired=0"),
                 wire.replace(b"cleanup=1", b"cleanup=0"),
                 wire.replace(b"dispatcher=0", b"dispatcher=1"),
                 wire.replace(b"dispatcher-checks=0", b"dispatcher-checks=1")]
    for changed in mutations:
        try:
            select_callbacks(changed)
        except ValueError:
            continue
        raise ValueError("Callback selector accepted altered evidence")
    return len(mutations)


def controls(expected, tag, count, owners=None):
    """Mutate actual observations, never derive an expected geometry table."""
    rejected = 0

    def reject(operation):
        nonlocal rejected
        try:
            operation()
        except (ValueError, IndexError):
            rejected += 1
            return
        raise ValueError("Redstone comparator accepted corrupted evidence")

    for changed in (expected[1:], expected + [expected[-1]],
                    [expected[0], *expected[4:7], *expected[1:4], *expected[7:]]):
        reject(lambda changed=changed: compare(changed, expected))
    # Mutate every independently observed scalar/vector/state layer in the first effect row.
    first = 2
    for column in range(1, 16):
        changed = expected.copy()
        row = changed[first].split("\t")
        if column in (10, 11, 15):
            words = row[column].split(",")
            width = 8 if column == 15 else 16
            words[0] = f"{int(words[0], 16) ^ 1:0{width}x}"
            row[column] = ",".join(words)
        elif column in (12, 13, 14):
            fields = row[column].split(",")
            key = "d" if column == 12 else "o"
            position = next(i for i, value in enumerate(fields) if value.startswith(key + "="))
            fields[position] = key + "=" + str(int(fields[position].split("=")[1]) + 1)
            row[column] = ",".join(fields)
        elif column in (1, 3, 6, 7, 8, 9):
            row[column] = str(int(row[column]) + 1)
        elif column == 4:
            row[column] = "false" if row[column] == "true" else "true"
        elif column == 5:
            row[column] = "NONE" if row[column] == "NULL" else "NULL"
        else:
            row[column] += "changed"
        changed[first] = "\t".join(row)
        reject(lambda changed=changed: compare(changed, expected))
    # Distinct packed position, UV, color and reserved/tail words from actual output.
    for word_index in (1, 2, 3, 4, 5, 6, 7, -1):
        changed = expected.copy()
        fields = changed[first].split("\t")
        words = fields[-1].split(",")
        words[word_index] = f"{int(words[word_index], 16) ^ 1:08x}"
        fields[-1] = ",".join(words)
        changed[first] = "\t".join(fields)
        reject(lambda changed=changed: compare(changed, expected))
    for index in (0, 1, 3):
        fields = expected[index].split("\t")
        for column in range(1, len(fields)):
            changed = expected.copy()
            modified = fields.copy()
            modified[column] += "changed"
            changed[index] = "\t".join(modified)
            reject(lambda changed=changed: compare(changed, expected))
    for old, new in (("begins=1", "begins=2"), ("draw=0", "draw=1"),
                     ("source-replacements=0", "source-replacements=1")):
        changed = expected.copy()
        changed[-1] = changed[-1].replace(old, new)
        reject(lambda changed=changed: compare(changed, expected))
    body = ("\n".join(expected) + "\n" + sample_native_marker(tag == "REDSTONE") + "\n").encode("ascii")
    if owners is not None:
        owner_fields = signed_owner_receipt(owners).split("\t")
        owner_mutations = [b"", owners + owners, owners.replace(b"\n", b"\r\n")]
        for column in range(1, len(owner_fields)):
            changed = owner_fields.copy()
            if column < 3:
                changed[column] = str(int(changed[column]) + 1)
            else:
                values = changed[column].split(",")
                # Each request identity, actual coordinate and first-equal slot
                # must match the separately captured Source owner observation.
                for component in (0, 1, 2, 3, 4, 5):
                    altered = values.copy()
                    altered[component] = str(int(altered[component]) + 1)
                    changed[column] = ",".join(altered)
                    owner_mutations.append(("\t".join(changed) + "\n").encode("ascii"))
                continue
            owner_mutations.append(("\t".join(changed) + "\n").encode("ascii"))
        for changed in owner_mutations:
            reject(lambda changed=changed: select_native(BOOTSTRAP + changed + body, tag, count, owners))
        body = owners + body
    select_native(BOOTSTRAP + body, tag, count, owners)
    for malformed in (body, BOOTSTRAP + body[:-1], BOOTSTRAP + body + b"unexpected\n",
                      BOOTSTRAP + body.replace(b"\n", b"\r\n"),
                      BOOTSTRAP + body.replace(b"cleanup=1", b"cleanup=0"),
                      BOOTSTRAP + body.replace(b"dispatcher=" + (b"1" if tag == "REDSTONE" else b"0"),
                                              b"dispatcher=" + (b"0" if tag == "REDSTONE" else b"1")),
                      BOOTSTRAP + body.replace(b"dispatcher-checks=" + (b"1" if tag == "REDSTONE" else b"0"),
                                              b"dispatcher-checks=" + (b"0" if tag == "REDSTONE" else b"1"))):
        reject(lambda malformed=malformed: select_native(malformed, tag, count, owners))
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--profiles", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--owners", type=Path, help="separate literal signed request-slot owner receipt")
    parser.add_argument("--layout", choices=("flat", "vertical", "capability", "signed", "nullable", "callbacks"), required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    callbacks = args.layout == "callbacks"
    if callbacks and (args.profiles is not None or args.reference is not None or args.owners is not None):
        parser.error("callbacks has no original profiles/reference operands")
    if not callbacks and (args.profiles is None or args.reference is None):
        parser.error("literal replay requires --profiles and --reference")
    if (args.layout == "signed") != (args.owners is not None):
        parser.error("--owners is required only for signed replay")
    profiles = None if callbacks else args.profiles.resolve(strict=True)
    reference = None if callbacks else args.reference.resolve(strict=True)
    owners_path = None if args.owners is None else args.owners.resolve(strict=True)
    owners = None if owners_path is None else owners_path.read_bytes()
    if owners is not None:
        signed_owner_receipt(owners)
    tag = {"flat": "REDSTONE", "vertical": "VERTICAL", "capability": "CAPABILITY",
           "signed": "SIGNED", "nullable": "NULLABLE",
           "callbacks": None}[args.layout]
    declared = [] if callbacks else profiles_at(profiles, args.layout)
    expected = [] if callbacks else reference_at(reference, declared, tag)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=f"redstone-{args.layout}-", dir=args.work_dir.resolve()))
    picking = module("redstone_native_world", ROOT / "tests/tools/client/test_picking.py")
    for name, body in picking.RESOURCES.items():
        path = output / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(body)
    world_inputs_path = ROOT / "tests/reference/client/redstone_world/signed/world_inputs.py"
    if args.layout == "signed":
        world_inputs = module("redstone_signed_world_inputs", world_inputs_path)
        world_inputs.world(output / "world")
    else:
        picking.authored_world(output / "world")
    selected = {executable, Path(__file__).resolve(),
                ROOT / "tests/tools/client/test_picking.py", ROOT / "tests/client/world/test_picking.c",
                ROOT / "tests/client/render/test_redstone_world.c",
                ROOT / "tests/client/render/redstone_callbacks.inc",
                ROOT / "tests/entity/player/test_construction.c",
                ROOT / "tests/tools/client/native_product_capture.py",
                *[p for p in (ROOT / "src").rglob("*") if p.is_file() and p.suffix in (".c", ".h", ".inc")],
                *sorted((ROOT / "include").rglob("*.h"))}
    if not callbacks:
        selected.update((profiles, reference))
    if args.layout == "signed":
        selected.update((world_inputs_path, ROOT / "tests/reference/client/picking/observe.py",
                         owners_path))
    before = {str(p): digest(p) for p in sorted(selected)}
    resources = inventory(output / "resources")
    report = {"status": "FAIL", "layout": args.layout,
              "profiles": None if callbacks else str(profiles),
              "reference": None if callbacks else str(reference),
              "owners": None if owners_path is None else str(owners_path),
              "inputs_before": before, "resources_before": resources,
              "world_before": inventory(output / "world"),
              "scope": "Constructed internal split-context callback stress: a Block from the ready World Registry "
                       "and a separate cold Code Registry; no ordinary same-namespace or source cross-loader proof. "
                       "Bound World/EW helper controls are native contracts; no original-row comparison."
                       if callbacks else "One fresh native direct CV.g cohort: literal original outer effects, full CV/NW primitives, "
                       "bounds and complete own-prior backing audits. Original JDI query events remain separate "
                       "source evidence; no native per-query trace, GL, terrain frame or full-game claim."}
    report["native_dispatcher_scope"] = ("One same-namespace mc_cv_render join after the56 direct calls, "
                                         "compared with first direct-g96words and full outside-append storage; "
                                         "no original whole-dispatcher claim. CHECK invocation count includes "
                                         "the native scene/buffer helper guards."
                                         if args.layout == "flat" else None)
    try:
        report["comparator_controls"] = callback_controls() if callbacks else controls(expected, tag, len(declared), owners)
        command = [str(executable), str(output / "resources"), str(output / "world"),
                   "-" if callbacks else str(profiles), args.layout]
        report["command"] = command
        products = module("redstone_native_products", ROOT / "tests/tools/client/native_product_capture.py")
        try:
            report["native_products"] = products.capture_native_products(command, output)
        except products.NativeProductCaptureError as error:
            report["native_products"] = error.receipt
            raise
        result = subprocess.run(command, capture_output=True, timeout=120, cwd=executable.parent)
        (output / "stdout.txt").write_bytes(result.stdout)
        (output / "stderr.txt").write_bytes(result.stderr)
        report.update(returncode=result.returncode, stdout_sha256=hashlib.sha256(result.stdout).hexdigest(),
                      stderr_sha256=hashlib.sha256(result.stderr).hexdigest())
        if result.returncode != 0 or result.stderr:
            raise ValueError("Native Redstone fixture failed: " + result.stderr.decode("utf-8", errors="backslashreplace"))
        if callbacks:
            report.update(status="PASS", original_calls=0,
                          native_callback_controls=select_callbacks(result.stdout))
        else:
            actual, native = select_native(result.stdout, tag, len(declared), owners)
            compare(actual, expected)
            report.update(status="PASS", compared_lines=len(actual), original_calls=len(declared),
                          native_controls=native)
            if owners is not None:
                report["native_owners"] = signed_owner_receipt(
                    result.stdout[len(BOOTSTRAP):len(BOOTSTRAP) + len(owners)])
    except BaseException as error:
        report["error"] = repr(error)
        if isinstance(error, subprocess.TimeoutExpired):
            (output / "stdout.txt").write_bytes(error.stdout or b"")
            (output / "stderr.txt").write_bytes(error.stderr or b"")
        raise
    finally:
        report["inputs_after"] = {p: digest(Path(p)) for p in before}
        report["resources_after"] = inventory(output / "resources")
        report["world_after"] = inventory(output / "world")
        same = before == report["inputs_after"] and resources == report["resources_after"]
        if not same:
            report.update(status="FAIL", error="Native replay source/executable/reference/resources changed")
        report["inputs_unchanged"] = same
        (output / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        if not same:
            raise ValueError(report["error"])
    outcome = "native-only Redstone callback controls" if callbacks else f"{len(declared)} exact original Redstone {args.layout} calls"
    print(f"PASS: {outcome}; "
          f"{report['comparator_controls']} corruption controls; {output / 'run.json'}")


if __name__ == "__main__":
    main()
