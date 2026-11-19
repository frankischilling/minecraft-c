"""Check native launcher diagnostics before the display or game directory opens."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def check(executable, directory):
    home = directory / 'home'
    home.mkdir()
    assets = directory / 'imported assets'
    environment = os.environ.copy()
    environment.update(HOME=str(home), APPDATA=str(home), LOCALAPPDATA=str(home))

    def run(arguments, code, message):
        result = subprocess.run([str(executable), *arguments], cwd=directory, env=environment,
                                capture_output=True, timeout=10)
        if result.returncode != code or message.encode('utf-8') not in result.stderr:
            raise RuntimeError(f'Expected exit {code} and {message!r}: {result.stderr!r}')
        if list(home.iterdir()):
            raise RuntimeError('Missing assets must fail before writing the game directory')

    run([], 2, 'Pass the imported asset folder containing jar/ to --assets.')
    arguments = ['--assets', assets.as_posix(), '--hidden', '--frames', '1']
    guidance = 'Import assets with --lwjgl-jar and pass the import root to --assets.'
    run(arguments, 1, f'Missing SDK icon {assets.as_posix()}/sdk/lwjgl-icon-16.rgba. {guidance}')
    icons = assets / 'sdk'
    icons.mkdir(parents=True)
    (icons / 'lwjgl-icon-16.rgba').write_bytes(bytes(16 * 16 * 4))
    run(arguments, 1, f'Missing SDK icon {assets.as_posix()}/sdk/lwjgl-icon-32.rgba. {guidance}')
    (icons / 'lwjgl-icon-32.rgba').write_bytes(b'too short')
    run(arguments, 1, f'Invalid SDK icon size for {assets.as_posix()}/sdk/lwjgl-icon-32.rgba.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='mc-startup-assets-') as work:
        check(arguments.executable.resolve(strict=True), Path(work))


if __name__ == '__main__':
    main()
