"""Exercise the public formatter transport, including a failed stdout pipe."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    checks = 0
    with tempfile.TemporaryDirectory(prefix='mc-float-format-') as directory:
        path = Path(directory) / 'inputs.txt'
        header = b'minecraft-c-float-format-inputs-v1\n'
        path.write_bytes(header + b'zero\t00000000\nnormal\t00800000\n')
        run = subprocess.run([str(executable), str(path)], capture_output=True, timeout=10)
        assert run.returncode == 0 and not run.stderr
        rows = [json.loads(line) for line in run.stdout.splitlines()]
        assert len(rows) == 4 and rows[0]['type'] == 'header'
        assert [(r['ordinal'], r['id'], r['input_bits'], r['text'], r['length']) for r in rows[1:-1]] == [
            (0, 'zero', '00000000', '0.0', 3),
            (1, 'normal', '00800000', '1.17549435E-38', 14)]
        assert rows[-1] == {'type': 'terminal', 'rows': 2, 'status': 0, 'rounding_unchanged': True}
        checks += 1
        for data in (b'', b'wrong\n', header + b'bad\txyz\n', header + b'row\t00000000',
                     header + b'bad id\t00000000\n'):
            path.write_bytes(data)
            run = subprocess.run([str(executable), str(path)], capture_output=True, timeout=10)
            assert run.returncode == 1 and not run.stderr, (data, run.returncode, run.stderr)
            checks += 1
        path.write_bytes(header + b'row\t3f800000\n' * 600)
        read_fd, write_fd = os.pipe()
        os.close(read_fd)
        previous = None
        try:
            options = {}
            if hasattr(signal, 'SIGPIPE'):
                # Exercise C stdio error returns rather than default SIGPIPE.
                previous = signal.signal(signal.SIGPIPE, signal.SIG_IGN)
                options['restore_signals'] = False
            run = subprocess.run([str(executable), str(path)], stdout=write_fd,
                                 stderr=subprocess.PIPE, timeout=10, **options)
            assert run.returncode == 1 and not run.stderr, (run.returncode, run.stderr)
            checks += 1
        finally:
            os.close(write_fd)
            if previous is not None:
                signal.signal(signal.SIGPIPE, previous)
    print(f'PASS: {checks} Float.toString probe transport controls')


if __name__ == '__main__':
    main()
