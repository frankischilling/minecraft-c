"""Verify authored placement identities against original ZIP/classfile bytes, without Java."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[4]
CLIENT_SHA256 = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SELECTED = {
    'activation': ('uu', 'a', '(Lfd;IIILgs;)Z'),
    'can_place_face': ('uu', 'a', '(Lfd;IIII)Z'),
    'can_place': ('uu', 'a', '(Lfd;III)Z'),
    'placed_face': ('uu', 'e', '(Lfd;IIII)V'),
    'placed_by': ('uu', 'a', '(Lfd;IIILls;)V'),
    'item_use_on_block': ('gm', 'a', '(Liz;Lgs;Lfd;IIII)Z'),
    'item_use': ('gm', 'a', '(Liz;Lfd;Lgs;)Liz;'),
    'item_metadata': ('gm', 'b', '(I)I'),
    'world_can_place': ('fd', 'a', '(IIIIZI)Z'),
    'controller_use_on_block': ('ob', 'a', '(Lgs;Lfd;Liz;IIII)Z'),
    'controller_use': ('ob', 'a', '(Lgs;Lfd;Liz;)Z'),
    'stack_use_on_block': ('iz', 'a', '(Lgs;Lfd;IIII)Z'),
    'stack_use': ('iz', 'a', '(Lfd;Lgs;)Liz;'),
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class ClassReader:
    def __init__(self, raw):
        require(len(raw) <= 1048576, 'bounded classfile')
        self.raw, self.at = raw, 0
        require(self.number(4) == 0xcafebabe, 'classfile magic')
        self.take(4)
        self.pool = [None] * self.number(2)
        index = 1
        while index < len(self.pool):
            tag = self.number(1)
            if tag == 1:
                self.pool[index] = (tag, self.take(self.number(2)).decode('utf8', 'replace'))
            elif tag in (7, 8, 16):
                self.pool[index] = (tag, self.number(2))
            elif tag in (3, 4, 9, 10, 11, 12, 18):
                self.take(4)
            elif tag in (5, 6):
                self.take(8)
                index += 1
            elif tag == 15:
                self.take(3)
            else:
                raise AssertionError('unexpected original constant-pool tag')
            index += 1
        self.access, this, parent = self.number(2), self.number(2), self.number(2)
        self.name = self.text(this)
        self.parent = self.text(parent) if parent else None

    def take(self, size):
        require(0 <= size <= len(self.raw) - self.at, 'complete classfile field')
        value = self.raw[self.at:self.at + size]
        self.at += size
        return value

    def number(self, size):
        return int.from_bytes(self.take(size), 'big')

    def text(self, index):
        value = self.pool[index]
        require(value is not None and value[0] in (1, 7, 8, 16), 'class/UTF8 constant')
        return value[1] if value[0] == 1 else self.text(value[1])

    def attributes(self):
        return [(self.text(self.number(2)), self.take(self.number(4)))
                for _ in range(self.number(2))]

    def code(self, wanted_name, wanted_descriptor):
        self.take(self.number(2) * 2)
        for _ in range(self.number(2)):
            self.take(6)
            self.attributes()
        found = None
        for _ in range(self.number(2)):
            self.take(2)
            name, descriptor = self.text(self.number(2)), self.text(self.number(2))
            attributes = self.attributes()
            if (name, descriptor) == (wanted_name, wanted_descriptor):
                for key, value in attributes:
                    if key == 'Code':
                        size, = struct.unpack_from('>I', value, 4)
                        require(len(value) >= 8 + size, 'complete selected Code')
                        found = value[8:8 + size]
        require(found is not None, 'selected original Code exists')
        return found


def verify(jar, metadata, registry):
    raw = jar.read_bytes()
    require(len(raw) == 1465375 and hashlib.sha256(raw).hexdigest() == CLIENT_SHA256,
            'exact original client archive')
    require(metadata['client_jar'] == {'path': 'b1.7.3.jar', 'bytes': len(raw),
                                     'sha256': CLIENT_SHA256}, 'archive metadata identity')
    spec = importlib.util.spec_from_file_location('placement_class_identity',
                  ROOT / 'tests/reference/client/notice/classfile.py')
    inspector = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inspector)
    parents, headers, values = {}, {}, {}
    with zipfile.ZipFile(jar) as archive:
        require(len(archive.namelist()) == len(set(archive.namelist())), 'unique archive members')
        for name in archive.namelist():
            if not name.endswith('.class'):
                continue
            value = archive.read(name)
            reader = ClassReader(value)
            require(reader.name + '.class' == name, 'actual archive class identity')
            parents[reader.name], headers[reader.name], values[reader.name] = reader.parent, reader, value
    require(len(parents) == metadata['archive_class_count'], 'complete archive class inventory')

    def descends(name, base):
        visited = set()
        while name:
            require(name not in visited, 'acyclic original class hierarchy')
            visited.add(name)
            if name == base:
                return True
            name = parents.get(name)
        return False

    wrappers = {'net/minecraft/client/Minecraft', 'iz', 'ra', 'ct', 'ln'}
    relevant = wrappers | {name for name in parents
                          if any(descends(name, root) for root in ('uu', 'gm', 'fd', 'ob'))}
    require(relevant == set(metadata['classes']), 'complete relevant hierarchy inventory')
    actual = {}
    for name in sorted(relevant):
        actual[name] = {**inspector.inspect_class(values[name]), 'super_class': parents[name],
                        'class_access': headers[name].access}
        require(actual[name] == metadata['classes'][name],
                'exact class and complete declared-method identities: ' + name)
    original = ClassReader(values['uu'])
    initializer = original.code('<clinit>', '()V')
    kinds = {}
    for pc, kind in re.findall(r'NEW\(\s*(\d+)\s*,\s*(MC_BLOCK_\w+)\s*\)', registry.read_text(encoding='utf-8')):
        pc = int(pc)
        require(pc + 3 <= len(initializer) and initializer[pc] == 187,
                'registry NEW site is original allocation opcode')
        name = original.text(int.from_bytes(initializer[pc+1:pc+3], 'big'))
        require(descends(name, 'uu') and kinds.setdefault(kind, name) == name,
                'original Block allocation mapping')
    require(kinds == metadata['block_kind_to_original_class'], 'C kind/original class binding')
    require(set(metadata['declaring_sets']) == set(SELECTED), 'complete selected dispatcher set')
    for label, (base, method, descriptor) in SELECTED.items():
        declared = []
        for name in sorted(relevant):
            if not descends(name, base):
                continue
            found = [row for row in actual[name]['methods']
                     if (row['name'], row['descriptor']) == (method, descriptor)]
            require(len(found) <= 1, 'unique original method descriptor')
            if found:
                declared.append({'class': name, 'method': found[0],
                    'block_kinds': sorted(k for k, v in kinds.items() if v == name)})
        require(metadata['declaring_sets'][label] == {'base': base, 'name': method,
                'descriptor': descriptor, 'declared': declared}, 'complete declaring set: ' + label)
        if base == 'uu':
            declarations = {row['class'] for row in declared}
            resolved = {}
            for kind, original_class in kinds.items():
                current = original_class
                while current not in declarations:
                    current = parents[current]
                resolved[kind] = current
            require(resolved == metadata['resolved_block_methods'][label],
                    'actual inherited dispatch for every Block kind: ' + label)
    require(jar.read_bytes() == raw, 'original archive unchanged')
    return {'status': 'PASS_STATIC_IDENTITIES', 'classes': len(relevant), 'block_kinds': len(kinds),
            'game_class_execution': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--pins', type=Path, default=Path(__file__).with_name('source-pins.json'))
    parser.add_argument('--registry', type=Path, default=ROOT / 'src/block/registry/registration.c')
    args = parser.parse_args()
    if not args.jar.is_file():
        print(json.dumps({'status': 'UNAVAILABLE', 'reason': 'Original client archive is missing'}))
        return 77
    try:
        result = verify(args.jar, json.loads(args.pins.read_text(encoding='utf-8')), args.registry)
    except (AssertionError, OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(json.dumps({'status': 'FAIL', 'reason': str(error)}))
        return 1
    print(json.dumps(result))
    return 0


if __name__ == '__main__':
    sys.exit(main())
