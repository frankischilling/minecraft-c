"""Check actual DK lists/reuse and Block words from unchanged original captures."""
import argparse
import importlib.util
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def fluid_reference_cases(placement):
    """Project captured rows without deriving geometry from the native renderer."""
    packet = HERE.parents[1] / "reference/client/fluid_world"
    spec = importlib.util.spec_from_file_location("compiled_fluid_packet", packet / "packet.py")
    authority = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(authority)
    authority.read_packet()
    profiles = [line.split() for line in (packet / "profiles.txt").read_text().splitlines() if line]
    if len(profiles) != 104 or placement not in (1, 2):
        raise RuntimeError("Closed captured fluid profile inventory")
    by_kind = {name: {} for name in ('FLUID_INPUT', 'FLUID_CELLS', 'FLUID')}
    for cohort in range(3):
        path = packet / f"selected-{cohort}.txt"
        for line in path.read_text(encoding='ascii').splitlines():
            fields = line.split('\t')
            if fields[0] not in by_kind:
                continue
            width = {'FLUID_INPUT': 9, 'FLUID_CELLS': 4, 'FLUID': 16}[fields[0]]
            if len(fields) != width:
                raise RuntimeError(f"Captured fluid row width: {path.name}")
            index = int(fields[1])
            if index in by_kind[fields[0]] or not 0 <= index < len(profiles):
                raise RuntimeError("Captured fluid row index inventory")
            by_kind[fields[0]][index] = fields
    if any(set(rows) != set(range(104)) for rows in by_kind.values()):
        raise RuntimeError("Complete executed-original fluid input/cell/word inventory")
    selected = []
    for index, profile in enumerate(profiles):
        operands = list(map(int, profile[1:]))
        if len(operands) != 13:
            raise RuntimeError("Closed original fluid operand schema")
        if operands[10] != placement:
            continue
        inputs, cells, row = (by_kind[kind][index] for kind in by_kind)
        if (list(map(int, inputs[6].split(','))) != operands or row[2] != profile[0] or
                int(inputs[3]) != operands[0] or inputs[4] != inputs[5] or
                operands[4:10] != [0, -1, 0, 0, 0, 0] or
                row[3:5] != ['true', 'NONE'] or not re.fullmatch(r'[a-z0-9-]+', row[2])):
            raise RuntimeError("Actual original default-CV integration row")
        before, after = int(row[5]), int(row[6])
        prior = dict(value.split('=', 1) for value in row[13].split(','))
        final = dict(value.split('=', 1) for value in row[14].split(','))
        vertices = int(final['h']) - int(prior['h'])
        words = row[15].split(',')[:after - before]
        actual_cells = [list(map(int, cell.split(','))) for cell in cells[2].split(';')]
        xyz = list(map(int, inputs[7].split(',')))
        wanted_xyz = [8, 64, 8] if placement == 1 else [3, 65, 3]
        if (not 0 < vertices <= 36 or vertices % 6 or after - before != vertices * 8 or
                len(words) != vertices * 8 or any(not re.fullmatch('[0-9a-f]{8}', w) for w in words) or
                len(actual_cells) != 125 or any(len(cell) != 7 for cell in actual_cells) or
                len({tuple(cell[:3]) for cell in actual_cells}) != 125 or xyz != wanted_xyz or
                final['l'] != 'true' or final['m'] != 'true' or final['n'] != 'false'):
            raise RuntimeError("Actual original emitted fluid geometry and entered cells")
        selected.append({'name': profile[0], 'id': operands[0], 'metadata': operands[1],
                         'pass': int(inputs[8]), 'sky_subtraction': 3, 'xyz': xyz,
                         'vertices': vertices, 'cells': actual_cells, 'words': words})
    if (placement == 1 and (len(selected) != 8 or
                           sorted(row['id'] for row in selected) != [8, 8, 9, 9, 10, 10, 11, 11]) or
            placement == 2 and (len(selected) != 2 or
                               sorted(row['id'] for row in selected) != [9, 11])):
        raise RuntimeError("Exact DK/loaded executed-original integration cohort")
    return selected


def write_block_reference(path, cases, marker="FLUID_DK_V1"):
    rows = [f"{marker} {len(cases)}"]
    for case in cases:
        rows.append(' '.join(map(str, ['CASE', case['name'], case['id'], case['metadata'],
                                      case['pass'], case['sky_subtraction'], *case['xyz'],
                                      case['vertices'], len(case['cells'])])))
        rows.extend(' '.join(map(str, cell)) for cell in case['cells'])
        rows.append(' '.join(case['words']))
    path.write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')


def ladder_reference_cases(placement):
    packet = HERE.parents[1] / 'reference/client/ladder_world'
    spec = importlib.util.spec_from_file_location('compiled_ladder_packet', packet / 'packet.py')
    authority = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(authority)
    cases = authority.projected_cases(placement)
    for case in cases:
        x, y, z = case['xyz']
        light = 1 if placement == 1 else 2
        case['sky_subtraction'] = 3
        case['cells'] = []
        # The frozen original producer clears all four Chunk planes, writes
        # one Ladder and authors its 3x3x3 light cube. Retain the same 5x5x5
        # projection schema used by the fluid controls, including zero guards.
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                for dz in range(-2, 3):
                    center = dx == dy == dz == 0
                    lit = max(abs(dx), abs(dy), abs(dz)) <= 1
                    case['cells'].append([x + dx, y + dy, z + dz, 65 if center else 0,
                                          case['metadata'] if center else 0,
                                          15 if lit and light == 1 else 0,
                                          15 if lit and light == 2 else 0])
    return cases


def crops_reference_cases(placement):
    packet = HERE.parents[1] / 'reference/client/crops_world'
    spec = importlib.util.spec_from_file_location('compiled_crops_packet', packet / 'packet.py')
    authority = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(authority)
    cases = authority.projected_cases(placement)
    for case in cases:
        x, y, z = case['xyz']
        light = 1 if placement == 1 else 2
        case['sky_subtraction'] = 3
        case['cells'] = []
        # The complete packet validates the original producer's cleared planes,
        # single Crops receiver and authored 3x3x3 light cube before projection.
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                for dz in range(-2, 3):
                    center = dx == dy == dz == 0
                    lit = max(abs(dx), abs(dy), abs(dz)) <= 1
                    case['cells'].append([x + dx, y + dy, z + dz, 59 if center else 0,
                                          case['metadata'] if center else 0,
                                          15 if lit and light == 1 else 0,
                                          15 if lit and light == 2 else 0])
    return cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    spec = importlib.util.spec_from_file_location("dk_authored_world", HERE / "test_picking.py")
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="chunk-compile-", dir=args.work_dir.resolve()))
    for name, data in inputs.RESOURCES.items():
        path = out / "resources" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    inputs.authored_world(out / "world")
    fluids = fluid_reference_cases(1)
    ladders = ladder_reference_cases(1)
    crops = crops_reference_cases(1)
    reference = out / 'block-original-inputs-and-words.txt'
    write_block_reference(reference, fluids + ladders + crops, marker='BLOCK_DK_V3')
    result = subprocess.run([str(executable), str(out / "resources"), str(out / "world"), str(reference)],
                            env=os.environ.copy(), capture_output=True, text=True, timeout=100)
    (out / "native.stdout").write_text(result.stdout, encoding="utf-8", newline="\n")
    (out / "native.stderr").write_text(result.stderr, encoding="utf-8", newline="\n")
    expected = json.loads((HERE.parents[1] / "reference/client/chunk_compile/finite.json").read_text())["rows"]
    actual = [line for line in result.stdout.splitlines() if line.startswith("DK_EMPTY ")]
    cactus = [line for line in result.stdout.splitlines() if line.startswith("DK_CACTUS ")]
    torches = [line for line in result.stdout.splitlines() if line.startswith("DK_TORCH ")]
    fluid_rows = [line for line in result.stdout.splitlines() if line.startswith("DK_FLUID ")]
    ladder_rows = [line for line in result.stdout.splitlines() if line.startswith("DK_LADDER ")]
    crops_rows = [line for line in result.stdout.splitlines() if line.startswith("DK_CROPS ")]
    fluid_expected = []
    for case in fluids:
        for stage in ('compile', 'reuse', 'dirty'):
            if stage == 'reuse':
                fluid_expected.append(f"DK_FLUID profile={case['name']} stage=reuse attempts=0 "
                                      f"same-definition=1 vertices={case['vertices']}")
            else:
                fluid_expected.append(f"DK_FLUID profile={case['name']} stage={stage} block={case['id']} "
                                      f"metadata={case['metadata']} pass={case['pass']} cv-return=48 "
                                      f"vertices={case['vertices']} exact-active-words=1 own-prior-inactive=1 "
                                      f"world-notification={int(stage == 'dirty')}")
    ladder_expected = []
    for case in ladders:
        for stage in ('compile', 'reuse', 'dirty'):
            if stage == 'reuse':
                ladder_expected.append(f"DK_LADDER profile={case['name']} stage=reuse attempts=0 "
                                       f"same-definition=1 vertices={case['vertices']}")
            else:
                ladder_expected.append(f"DK_LADDER profile={case['name']} stage={stage} block=65 "
                                       f"metadata={case['metadata']} pass=0 cv-return=163 "
                                       f"vertices={case['vertices']} exact-active-words=1 own-prior-inactive=1 "
                                       f"world-notification={int(stage == 'dirty')}")
    crops_expected = []
    for case in crops:
        for stage in ('compile', 'reuse', 'dirty'):
            if stage == 'reuse':
                crops_expected.append(f"DK_CROPS profile={case['name']} stage=reuse attempts=0 "
                                      f"same-definition=1 vertices={case['vertices']}")
            else:
                crops_expected.append(f"DK_CROPS profile={case['name']} stage={stage} block=59 "
                                      f"metadata={case['metadata']} pass=0 cv-return=98 "
                                      f"vertices={case['vertices']} exact-active-words=1 own-prior-inactive=1 "
                                      f"world-notification={int(stage == 'dirty')}")
    cactus_expected = [
        "DK_CACTUS stage=compile block=81 last-scanned=0 cv-return=65 pass=0 vertices=66 inset-vertices=24 dirty=0 pass0-empty=0 pass1-empty=1",
        "DK_CACTUS stage=reuse attempts=0 same-definition=1 vertices=66",
        "DK_CACTUS stage=dirty block=81 last-scanned=0 cv-return=65 pass=0 vertices=66 inset-vertices=24 dirty=0 pass0-empty=0 pass1-empty=1",
    ]
    torch_expected = []
    for metadata in range(1, 6):
        for stage in ('compile', 'reuse', 'dirty'):
            if stage == 'reuse':
                torch_expected.append(f"DK_TORCH stage=reuse metadata={metadata} attempts=0 same-definition=1 vertices=66")
            else:
                torch_expected.append(f"DK_TORCH stage={stage} block=50 metadata={metadata} cv-return=114 pass=0 vertices=66 "
                                      "torch-vertices=30 exact-position-uv-color=1 dirty=0 pass0-empty=0 pass1-empty=1")
    if (result.returncode or result.stderr or actual != expected or cactus != cactus_expected or
            torches != torch_expected or fluid_rows != fluid_expected or ladder_rows != ladder_expected
            or crops_rows != crops_expected
            or not result.stdout.splitlines()
            or not result.stdout.splitlines()[-1].startswith("Source DK: ")):
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    print(result.stdout.splitlines()[-1])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
