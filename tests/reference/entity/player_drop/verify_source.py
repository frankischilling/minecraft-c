"""Verify original drop method identities and references without executing game classes."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
READER = HERE.parents[1] / 'client/resource_lookup_common/offline_classfile.py'
spec = importlib.util.spec_from_file_location('player_drop_classfile', READER)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
JAR_SHA256 = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SELECTED = {
    'net/minecraft/client/Minecraft': [('k', '()V')],
    'dc': [],
    'gs': [('D', '()V'), ('a', '(Liz;)V'), ('a', '(Liz;Z)V'), ('a', '(Lhl;)V'), ('w', '()F')],
    'ix': [('a', '(II)Liz;')],
    'iz': [('a', '(I)Liz;')],
    'hl': [('<init>', '(Lfd;DDDLiz;)V')],
    'fd': [('b', '(Lsn;)Z')],
    'in': [('a', '(F)F'), ('b', '(F)F')],
}


def make_pins(jar):
    data = jar.read_bytes()
    if len(data) != 1465375 or hashlib.sha256(data).hexdigest() != JAR_SHA256:
        raise ValueError('Exact original client archive required')
    with zipfile.ZipFile(jar) as archive:
        classes = {name: reader.read_class(archive.read(name + '.class')) for name in SELECTED}

    def method(name, key):
        return next(m for m in classes[name]['methods']
                    if (m['name'], m['descriptor']) == key)

    result = {'scope': 'Offline original class/Code identities and resolved references; no runtime observations.',
              'jar_bytes': len(data), 'jar_sha256': JAR_SHA256, 'classes': {}}
    for name, keys in SELECTED.items():
        value = classes[name]
        inventory = [{k: m[k] for k in ('name', 'descriptor', 'access')} for m in value['methods']]
        selected = []
        for key in keys:
            m = method(name, key)
            selected.append({k: m[k] for k in ('name', 'descriptor', 'access', 'code_bytes',
                'code_sha256', 'code_attribute_sha256', 'max_stack', 'max_locals')})
        result['classes'][name] = {'class_bytes': value['class_bytes'],
            'class_sha256': value['class_sha256'], 'superclass': value['super'],
            'method_count': len(inventory), 'method_inventory_sha256': hashlib.sha256(
                json.dumps(inventory, sort_keys=True, separators=(',', ':')).encode()).hexdigest(),
            'selected_methods': selected}
    resolved = {}
    for key in SELECTED['gs']:
        name = 'dc'
        while name in classes:
            if any((m['name'], m['descriptor']) == key for m in classes[name]['methods']):
                resolved['dc.' + key[0] + key[1]] = name
                break
            name = classes[name]['super']
        else:
            raise ValueError('Unresolved concrete LocalPlayer drop declaration')
    result['resolved_declarations'] = resolved
    references = {}
    for name, key, pcs in [
        ('net/minecraft/client/Minecraft', ('k', '()V'), [812]),
        ('gs', ('D', '()V'), [13, 17]),
        ('gs', ('a', '(Liz;Z)V'), [5, 26, 36, 58, 70, 83, 97, 132, 146, 168, 181, 203,
                                  225, 241, 255, 275, 282, 302, 315, 319, 323]),
        ('gs', ('a', '(Lhl;)V'), [5]),
    ]:
        for pc in pcs:
            instruction = next(i for i in method(name, key)['instructions'] if i['pc'] == pc)
            references[f'{name}.{key[0]}{key[1]}:{pc}'] = {
                'operation': instruction['name'], 'reference': instruction['reference']}
    result['selected_references'] = references
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--pins', type=Path, default=HERE / 'source-pins.json')
    args = parser.parse_args()
    if not args.jar.is_file():
        print('UNAVAILABLE: original client archive is missing')
        return 77
    try:
        if make_pins(args.jar) != json.loads(args.pins.read_text(encoding='utf-8')):
            raise ValueError('Player drop original identities/references differ')
    except (AssertionError, OSError, ValueError, KeyError, StopIteration, zipfile.BadZipFile) as error:
        print('FAIL: ' + str(error))
        return 1
    print('PASS: Player drop original inventories, inheritance, Code and invocation references')
    return 0


if __name__ == '__main__':
    sys.exit(main())
