"""Authored real-file color inputs; no observer or captured answers.

Unexecuted source-stage helper. Use supplied imported resources and a NEW output
directory. Reuse the exact existing genuine Pack ZIP-input writer.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import shutil
import struct
import sys
import zlib

sys.dont_write_bytecode = True
PACK_INPUTS_SHA256 = 'e3749c48bd0230296194252569fae39226f2f14790907b479b82ba20531ea82d'


def png(seed):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    def pixel(x, y):
        alpha = 0 if (x + y + seed) % 3 == 0 else 255
        return bytes((x * 31 + seed, y * 67 + seed, x + y + seed, alpha))
    raw = b''.join(b'\0' + b''.join(pixel(x, y) for x in range(7)) for y in range(3))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 7, 3, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def prepare(imported, output, pack_inputs):
    if output.exists():
        raise ValueError('preserve existing inputs: output must be a new path')
    if hashlib.sha256(pack_inputs.read_bytes()).hexdigest() != PACK_INPUTS_SHA256:
        raise ValueError('Pack ZIP writer source identity mismatch')
    spec = importlib.util.spec_from_file_location('actual_pack_inputs', pack_inputs)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    resources = output / 'resources'
    names = ('pack.png', 'gui/background.png', 'font.txt', 'misc/watercolor.png',
             'misc/grasscolor.png', 'misc/foliagecolor.png')
    for name in names:
        target = resources / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(imported / name, target)
    broken = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + b'\0'
    (resources / 'lookup-truncated.png').write_bytes(broken)
    for selected in ('default', 'zip'):
        game = output / selected
        (game / 'texturepacks').mkdir(parents=True)
        value = 'Default' if selected == 'default' else 'color-test.zip'
        (game / 'options.txt').write_bytes(('skin:' + value + '\n').encode())
    helper.write_zip(output / 'zip/texturepacks/color-test.zip', [
        ('pack.txt', b'Authored color pack\nGenuine ZIP stream input\n'),
        ('pack.png', (resources / 'pack.png').read_bytes()),
        ('misc/watercolor.png', png(1)), ('misc/grasscolor.png', png(2)),
        ('misc/foliagecolor.png', png(3)), ('lookup-truncated.png', broken)])
    return resources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('imported', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--pack-inputs', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.imported, args.output, args.pack_inputs))


if __name__ == '__main__':
    main()
