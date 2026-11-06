"""Run real client World construction/meshing/drawing on authored resources."""

import argparse
import json
from pathlib import Path
import tempfile

from test_startup import authored_png, need, options, resources, run_case, tree


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='mc-world-inspection-', dir=args.work_dir.resolve(strict=True)))
    assets = root / 'authored-import'
    resources(assets)
    jar = assets / 'jar'
    (jar / 'lang/stats_US.lang').write_bytes(
        b'stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n'
        b'stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n')
    images = {
        'terrain.png': authored_png(256, 256, lambda x, y:
            (40 + (x // 16 * 13) % 200, 45 + (y // 16 * 17) % 200,
             35 + ((x + y) // 4 * 7) % 210, 255)),
        'misc/grasscolor.png': authored_png(256, 256, lambda x, y: (80, 180, 55, 255)),
        'misc/foliagecolor.png': authored_png(256, 256, lambda x, y: (45, 155, 75, 255)),
    }
    for name, raw in images.items():
        path = jar / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
    receipt = {'scope': 'authored-native-World-session-mesh-GPU-App', 'passed': False,
               'assets_before': tree(assets), 'cases': {}, 'original_rendering_claim': False}
    try:
        captures = []
        for name, anaglyph in [('first', False), ('repeat', False), ('anaglyph', True)]:
            world = root / (name + '-world')
            need(not world.exists(), 'fresh owned World path')
            result, pixels = run_case(args.executable.resolve(strict=True), assets, root, name,
                options(1, anaglyph), None, [],
                extra_arguments=('--inspect-world', str(world), '--world-seed', '23'),
                startup_lines=(b'16 achievements', b'151 recipes'), timeout_seconds=60)
            receipt['cases'][name] = result
            need(result['passed'] and pixels is not None, 'actual World inspection: ' + name)
            captures.append(pixels)
            state = tree(world)
            receipt['cases'][name]['world_after'] = state
            need(set(state['directories']) <= {'data', 'region'}, 'bounded World directory profile')
            need('session.lock' in state['files'] and state['files']['session.lock']['bytes'] == 8,
                 'real handler session token')
            for filename, pin in state['files'].items():
                need(filename == 'session.lock' or
                     (filename.startswith('region/r.') and filename.endswith('.mcr') and pin['bytes'] == 8192),
                     'inspection creates only session token and empty region headers')
        need(captures[0] == captures[1], 'same-seed fresh native World pixel repeat')
        need(captures[0] != captures[2], 'loaded anaglyph reaches actual terrain rendering')
        need(tree(assets) == receipt['assets_before'], 'unchanged authored resources')
        receipt['passed'] = True
    finally:
        (root / 'world-inspection.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
        print('World inspection artifacts:', root)
    print('PASS: three actual World/client captures and two native pixel relations')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
