"""Compare native simple folds and counted UTF16 comparison with an actual JDK17."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import traceback

HERE = Path(__file__).resolve().parent
ROOT = next(path for path in HERE.parents if (path / 'CMakeLists.txt').is_file())
FOLD_SHA256 = '6db4c52baaef252f2bc0fa4536528871ff077f4cf0338c81228edda1ee32d628'
PAIR_COUNT = 1306160


def need(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'Duplicate JSON key')
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='string-case-', dir=args.work_dir)).resolve()
    report = {'status': 'FAIL', 'scope': 'Actual JDK17 scalar folds and finite counted UTF16 pairs'}
    protected, before, failure = [], {}, None
    env = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'CLASSPATH'):
        env.pop(key, None)

    def run(command, name, success=True):
        command = [str(value) for value in command]
        result = subprocess.run(command, capture_output=True, env=env, timeout=90)
        (output / (name + '.stdout')).write_bytes(result.stdout)
        (output / (name + '.stderr')).write_bytes(result.stderr)
        (output / (name + '.json')).write_text(json.dumps({
            'command': command, 'returncode': result.returncode,
            'stdout_sha256': hashlib.sha256(result.stdout).hexdigest(),
            'stderr_sha256': hashlib.sha256(result.stderr).hexdigest(),
        }, indent=2) + '\n', encoding='utf-8', newline='\n')
        if success:
            need(result.returncode == 0 and not result.stderr, 'Completed native/reference ' + name)
        else:
            need(result.returncode != 0, 'Refused malformed comparison input ' + name)
        return result

    try:
        executable = args.executable.resolve(strict=True)
        need(args.java is not None and args.javac is not None, 'Actual JDK17 tools required')
        java = Path(shutil.which(args.java) or args.java).resolve(strict=True)
        javac = Path(shutil.which(args.javac) or args.javac).resolve(strict=True)
        need(java.parent.samefile(javac.parent), 'Matching selected JDK17 tools')
        for tool, name in ((java, 'java-version'), (javac, 'javac-version')):
            result = subprocess.run([str(tool), '-version'], capture_output=True, env=env, timeout=20)
            (output / (name + '.stdout')).write_bytes(result.stdout)
            (output / (name + '.stderr')).write_bytes(result.stderr)
            text = (result.stdout + result.stderr).decode('utf-8', errors='strict')
            need(result.returncode == 0 and re.search(r'(?:version\s+"|javac\s+)17\.', text),
                 'Actual selected JDK17 ' + name)
        jvm = (java.parent / 'server/jvm.dll' if os.name == 'nt' else
               java.parent.parent / 'lib/server/libjvm.so')
        protected = [executable, java, javac, jvm.resolve(strict=True),
                     (java.parent.parent / 'lib/modules').resolve(strict=True),
                     HERE / 'StringCaseReference.java', Path(__file__), HERE / 'generate_table.py',
                     ROOT / 'src/core/java/string_case.c', ROOT / 'src/core/java/string_case_table.inc',
                     ROOT / 'src/core/java/string_case_internal.h', ROOT / 'include/mc/core/java/string_case.h',
                     ROOT / 'tests/core/java/test_string_case.c']
        before = {str(path): digest(path) for path in protected}
        classes, data = output / 'classes', output / 'original'
        classes.mkdir()
        data.mkdir()
        run([javac, '-d', classes, HERE / 'StringCaseReference.java'], 'compile')
        class_before = {str(path): digest(path) for path in classes.rglob('*.class')}
        original = run([java, '-cp', classes, 'StringCaseReference', data], 'original')
        need(original.stdout == f'JDK17 string case: codepoints=1114112 pairs={PAIR_COUNT}\n'.encode() or
             original.stdout == f'JDK17 string case: codepoints=1114112 pairs={PAIR_COUNT}\r\n'.encode(),
             'Complete declared original inventory')
        report['runtime'] = json.loads((data / 'runtime.json').read_text(), object_pairs_hook=unique)
        need(digest(data / 'fold.bin') == FOLD_SHA256, 'Qualified complete JDK17 fold data')
        report['original_fold_sha256'] = digest(data / 'fold.bin')
        report['original_pairs_sha256'] = digest(data / 'pairs.bin')
        unit = run([executable], 'native-unit')
        need(re.fullmatch(rb'String case: [1-9][0-9]* checks\r?\n', unit.stdout), 'Native finite controls')
        native_fold = output / 'native-fold.bin'
        result = run([executable, '--fold-output', native_fold], 'native-fold')
        need(result.stdout in (b'String case folds: 1114112 values\n', b'String case folds: 1114112 values\r\n'),
             'Native complete scalar inventory')
        need(native_fold.read_bytes() == (data / 'fold.bin').read_bytes(), 'Exact 1114112 native/original folds')
        pairs = run([executable, '--pairs', data / 'pairs.bin'], 'native-pairs')
        need(pairs.stdout in (f'String case pairs: {PAIR_COUNT} checks\n'.encode(),
                              f'String case pairs: {PAIR_COUNT} checks\r\n'.encode()),
             'Exact complete pair inventory')
        regenerated = output / 'regenerated-table.inc'
        run([sys.executable,
             '-B', HERE / 'generate_table.py', '--fold', data / 'fold.bin', '--output', regenerated],
            'regenerate-table')
        need(regenerated.read_bytes() == (ROOT / 'src/core/java/string_case_table.inc').read_bytes(),
             'Exact generated runtime table')
        header = struct.pack('>II', 0x53434931, 1)
        one_pair = struct.pack('>IH', 1, 0x03a3) + struct.pack('>IH', 1, 0x03c2) + b'\x01'
        corruptions = {
            'wrong-answer': header + one_pair[:-1] + b'\x00',
            'wrong-magic': b'BAD!' + header[4:] + one_pair,
            'truncated': header + one_pair[:-1],
            'trailing': header + one_pair + b'X',
            'missing-record': struct.pack('>II', 0x53434931, 2) + one_pair,
            'invalid-Boolean': header + one_pair[:-1] + b'\x02',
            'oversized-length': header + struct.pack('>I', 65),
        }
        for name, value in corruptions.items():
            path = output / (name + '.bin')
            path.write_bytes(value)
            run([executable, '--pairs', path], 'control-' + name, success=False)
        report['helper_classes_unchanged'] = class_before == {str(path): digest(path) for path in classes.rglob('*.class')}
        need(report['helper_classes_unchanged'], 'Unchanged authored helper products')
        report.update(status='PASS', codepoints=1114112, pairs=PAIR_COUNT,
                      negative_controls=list(corruptions))
    except Exception as error:
        failure = error
        report['error'] = f'{type(error).__name__}: {error}'
        (output / 'driver-error.txt').write_text(traceback.format_exc(), encoding='utf-8', newline='\n')
    finally:
        try:
            after = {str(path): digest(path) for path in protected}
        except Exception as error:
            after = {'fingerprint_error': f'{type(error).__name__}: {error}'}
        report['input_before'] = before
        report['input_after'] = after
        report['inputs_unchanged'] = bool(before) and before == after
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
            if failure is None:
                failure = ValueError('Protected input fingerprint changed')
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('JDK17 string case ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
