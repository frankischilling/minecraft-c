"""UNRUN authored real files for typed Pack controls; no original assets/answers."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import sys
import zipfile

sys.dont_write_bytecode = True


def module(path, name, expected):
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('fixture helper source pin: ' + str(path))
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def prepare(output, pack_inputs, image_inputs, pins):
    if output.exists():
        raise ValueError('preserve old fixtures; output must be a new owned path')
    source_pins = json.loads(pins.read_text(encoding='utf8'))
    packs = module(pack_inputs, 'same_authored_pack_zip_inputs', source_pins['pack_inputs'])
    images = module(image_inputs, 'same_authored_texture_png_inputs', source_pins['image_inputs'])
    resources = output / 'resources'
    resources.mkdir(parents=True)
    (resources / 'pack.png').write_bytes(images.png())
    (output / 'missing-resources').mkdir()
    cases = ['empty', 'absent', 'non-directory', 'mkdir-false', 'stored', 'deflated',
             'metadata-limit', 'removed', 'null-app', 'null-settings', 'missing-icon', 'bound']
    for kind in cases:
        parent = output / kind
        if kind == 'mkdir-false':
            parent.write_bytes(b'Authored regular parent: real mkdirs returns false.\n')
            continue
        parent.mkdir()
        if kind != 'absent':
            if kind == 'non-directory':
                (parent / 'texturepacks').write_bytes(b'Authored regular non-directory.\n')
            else:
                (parent / 'texturepacks').mkdir()
        if kind in ('stored', 'deflated', 'metadata-limit', 'removed'):
            name = 'deflated' if kind == 'deflated' else 'stored'
            method = zipfile.ZIP_DEFLATED if name == 'deflated' else zipfile.ZIP_STORED
            packs.write_zip(parent / 'texturepacks' / (name + '.zip'),
                            [('a.txt', b'first'), ('pack.txt', b'Authored Pack\nReal ZIP stream\n')],
                            method)
            (parent / 'options.txt').write_bytes(('skin:' + name + '.zip\n').encode('ascii'))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--pack-inputs', type=Path, required=True)
    parser.add_argument('--image-inputs', type=Path, required=True)
    parser.add_argument('--pins', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.output, args.pack_inputs, args.image_inputs, args.pins))


if __name__ == '__main__':
    main()
