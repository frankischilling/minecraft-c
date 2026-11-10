"""Run authored remaining LivingEntity death/drop controls without original assets."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='remaining-', dir=args.work_dir)).resolve()
    resources, world = output / 'resources', output / 'world'
    world.mkdir()
    for name, data in {
        'lang/en_US.lang': (b'stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n'
                            b'stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n'),
        'lang/stats_US.lang': b'# Authored statistics resource\n',
        'achievement/map.txt': b'',
    }.items():
        path = resources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    result = subprocess.run([str(args.executable.resolve(strict=True)), str(resources), str(world)],
                            capture_output=True, timeout=150)
    (output / 'stdout.txt').write_bytes(result.stdout)
    (output / 'stderr.txt').write_bytes(result.stderr)
    print(result.stdout.decode('utf-8', errors='strict'), end='')
    print(result.stderr.decode('utf-8', errors='strict'), end='')
    if result.returncode or result.stderr or not re.search(
            rb'PASS: [1-9][0-9]* remaining Living death checks', result.stdout):
        raise RuntimeError('Native remaining death control failed: ' + str(output))


if __name__ == '__main__':
    main()
