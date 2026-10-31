"""Future private input preparation; authored source, never a game-output oracle."""
import argparse
import hashlib
from pathlib import Path
import sys
import zipfile
sys.dont_write_bytecode = True
from capture import load, write
from protocol import fingerprint, need


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root','client-jar','imported','output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    need(fingerprint(args.client_jar) == {'bytes':1465375,'sha256':'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'}, 'actual supplied JAR')
    need(not args.output.exists() and not args.output.resolve().is_relative_to(root), 'fresh private destination outside source')
    args.output.mkdir(parents=True)
    source_before = {}
    helpers = {}
    for name in ('texture','color'):
        path = root / ('tests/reference/client/' + name + '_lookup/fixtures.py')
        source_before[str(path)] = fingerprint(path)
        helpers[name] = load('real_' + name + '_lookup_inputs', path)
    pack = root / 'tests/reference/client/texture_packs/inputs.py'
    source_before[str(pack)] = fingerprint(pack)
    resources = {name:helper.prepare(args.imported.resolve(strict=True), args.output / name, pack)
                 for name,helper in helpers.items()}
    expected = {}
    with zipfile.ZipFile(args.client_jar) as archive:
        for name in ('lang/en_US.lang','lang/stats_US.lang','achievement/map.txt'):
            raw = archive.read(name)
            expected[name] = {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
    owner = load('actual_constructor_resource_copy', root / 'tests/reference/client/statistics/support.py')
    for resource_root in resources.values():
        owner.extract_resources(args.client_jar, resource_root, expected)
    need(all(fingerprint(Path(path)) == value for path,value in source_before.items()), 'real input helper sources unchanged')
    write(args.output / 'INPUTS.json', {'scope':'Genuine authored File/ZIP/PNG plus supplied constructor class resources; no captured output',
        'client_jar':fingerprint(args.client_jar), 'helper_sources':source_before, 'constructor_resources':expected,
        'files':{path.relative_to(args.output).as_posix():fingerprint(path)
                 for path in sorted(args.output.rglob('*')) if path.is_file()}})


if __name__ == '__main__':
    main()
