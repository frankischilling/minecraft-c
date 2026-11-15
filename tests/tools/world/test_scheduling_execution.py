"""Run retained scheduling controls on a fresh native World and queue owners."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='retained-scheduling-', dir=args.work_dir.resolve()))
    capture_path = ROOT / 'tests/reference/client/controller/capture.py'
    spec = importlib.util.spec_from_file_location('retained_scheduling_capture', capture_path)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    paths = [executable, Path(__file__).resolve(), capture_path,
             ROOT / 'tests/world/runtime/test_scheduling_execution.c',
             ROOT / 'tests/world/runtime/test_scheduling.c',
             ROOT / 'src/world/runtime/scheduling.c',
             ROOT / 'src/world/runtime/scheduling_execution_internal.h',
             ROOT / 'src/world/runtime/mutation/world.c',
             ROOT / 'src/world/runtime/mutation/world_execution_internal.h',
             ROOT / 'src/world/runtime/mutation/chunk.c',
             ROOT / 'src/world/runtime/mutation/chunk_execution_internal.h',
             ROOT / 'src/world/runtime/mutation/fluids.c',
             ROOT / 'cmake/tests/world_scheduling_execution.cmake',
             ROOT / 'src/world/runtime/mutation/falling.c']
    before = {str(path): digest(path) for path in paths}
    report = {'schema': 'RETAINED_SCHEDULING_V1', 'status': 'FAIL',
              'scope': 'Native deferred queue/sequence/radius effects, reached failures, immediate tick frontier and nonzero/same-ID Chunk metadata plus retained StillWater conversion; no original comparison or tick-loop qualification.',
              'input_before_sha256': before}
    try:
        command = [str(executable), 'retained-scheduling', str(output / 'world').encode().hex()]
        receipt = capture.capture(command, output, 'native', 90)
        report['native'] = receipt
        capture.require_success(receipt, empty_stderr=True)
        lines = (output / receipt['stdout_file']).read_text(encoding='ascii').splitlines()
        if (len(lines) != 3 or lines[:2] != ['16 achievements', '151 recipes'] or
                re.fullmatch(r'PASS: [1-9][0-9]* retained scheduling checks', lines[2]) is None):
            raise ValueError('Complete retained scheduling native output required')
        report['status'] = 'PASS_NATIVE_CONTROLS_ONLY'
    finally:
        report['input_after_sha256'] = {str(path): digest(path) for path in paths}
        if before != report['input_after_sha256']:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if before != report['input_after_sha256']:
            raise ValueError('Retained scheduling inputs changed during run')
    print('Retained scheduling controls pass: ' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
