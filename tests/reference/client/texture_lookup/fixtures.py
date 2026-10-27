"""Prepare small authored real-file Default/ZIP test inputs; not an observer.

Authored/unexecuted in this source stage. Takes an existing user-imported resource
root and a new empty output directory. Never emits captured answers/classes/JARs.
"""
from pathlib import Path
import argparse
import importlib.util
import shutil
import struct
import sys
import zlib
sys.dont_write_bytecode = True


def png():
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + b''.join(bytes((x * 31, y * 67, x + y, 255)) for x in range(7)) for y in range(3))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 7, 3, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


def prepare(imported, output, pack_inputs):
    if output.exists():
        raise ValueError('output must be a new path; preserve existing inputs')
    spec = importlib.util.spec_from_file_location('actual_pack_authored_inputs', pack_inputs)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    resources = output / 'resources'
    for name in ('pack.png', 'gui/background.png', 'font.txt'):
        target = resources / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(imported / name, target)
    # A real malformed PNG causes the actual scoped ImageIO read failure;
    # no replacement callback supplies an IOException or completed image.
    broken = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + b'\0'
    (resources / 'lookup-truncated.png').write_bytes(broken)
    for selected in ('default', 'zip'):
        game = output / selected
        (game / 'texturepacks').mkdir(parents=True)
        (game / 'options.txt').write_bytes(('skin:' + ('Default' if selected == 'default' else 'lookup-test.zip') + '\n').encode())
    helper.write_zip(output / 'zip/texturepacks/lookup-test.zip', [
        ('pack.txt', b'Authored lookup pack\nGenuine ZIP stream input\n'),
        ('pack.png', (resources / 'pack.png').read_bytes()),
        ('gui/background.png', png()), ('lookup-truncated.png', broken)])
    return resources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('imported', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--pack-inputs', type=Path, required=True,
                        help='Exact existing tests/reference/client/texture_packs/inputs.py')
    args = parser.parse_args()
    print(prepare(args.imported, args.output, args.pack_inputs))


if __name__ == '__main__':
    main()
