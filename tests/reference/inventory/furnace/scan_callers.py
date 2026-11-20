"""Archive-only candidate inventory for direct DW.a()V/BM.a()V references."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def references(data):
    at = 8

    def u2():
        nonlocal at
        value, = struct.unpack_from('>H', data, at)
        at += 2
        return value

    count = u2()
    pool = [None] * count
    index = 1
    while index < count:
        tag = data[at]
        at += 1
        if tag == 1:
            size = u2()
            pool[index] = data[at:at + size].decode('utf8', errors='replace')
            at += size
        elif tag in (7, 8, 16, 19, 20):
            pool[index] = (tag, u2())
        elif tag in (9, 10, 11, 12, 17, 18):
            pool[index] = (tag, u2(), u2())
        elif tag in (3, 4):
            at += 4
        elif tag in (5, 6):
            at += 8
            index += 1
        elif tag == 15:
            at += 3
        else:
            raise ValueError('class constant-pool tag')
        index += 1
    result = []
    for index, row in enumerate(pool):
        if type(row) is tuple and row[0] in (10, 11):
            owner = pool[pool[row[1]][1]]
            name = pool[pool[row[2]][1]]
            descriptor = pool[pool[row[2]][2]]
            if owner in ('dw', 'bm') and name == 'a' and descriptor == '()V':
                result.append({'pool_index': index, 'owner': owner, 'name': name, 'descriptor': descriptor})
    return result


def scan(jar):
    if hashlib.sha256(jar.read_bytes()).hexdigest() != JAR:
        raise ValueError('verified original archive')
    with zipfile.ZipFile(jar) as archive:
        classes = sorted(name for name in archive.namelist() if name.endswith('.class'))
        rows = []
        for name in classes:
            raw = archive.read(name)
            refs = references(raw)
            if refs:
                rows.append({'class': name[:-6], 'class_sha256': hashlib.sha256(raw).hexdigest(), 'references': refs})
    return {'schema': 1, 'client_jar_sha256': JAR, 'classes_scanned': len(classes),
            'scope': 'Exhaustive original archive constant-pool candidates; inspect original javap for reached invoke sites. No runtime caller claim.',
            'candidates': rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(scan(args.jar), indent=2))
