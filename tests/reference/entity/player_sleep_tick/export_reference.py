"""Check a fresh private reproduction against sealed original finite bytes.

This tool never replaces the canonical packet or rewrites historical producer
identities. Its current hash belongs only to current_reproduction_sha256.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = next(p for p in HERE.parents
                if (p / 'tests/reference/client/controller/capture.py').is_file())
    source, output = args.private_capture.resolve(), args.output.resolve()
    private = (root / '.local').resolve()
    if not (source.is_relative_to(private) and output.is_relative_to(private) and
            source != private and output != private and not output.exists()):
        raise ValueError('fresh private source/output descendants required')
    spec = importlib.util.spec_from_file_location('sleep_parent_packet_export', HERE / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    original = packet.read_reference()
    historical = packet.load(HERE / 'provenance.json')
    receipt = packet.load(source / 'capture.json')
    if not (receipt['status'] == 'PASS_ORIGINAL_PREFIX_COMPONENTS_ONLY' and
            receipt['profiles_per_vm'] == 36 and receipt['fresh_target_vms'] == 2 and
            receipt['all_inputs_unchanged'] is True and
            receipt['input_before_sha256'] == receipt['input_after_sha256'] and
            receipt['helper_before_sha256'] == receipt['helper_after_sha256'] and
            receipt['legacy_before_sha256'] == receipt['legacy_after_sha256']):
        raise ValueError('two fresh complete unchanged-input reproduction VMs required')
    # Re-read the actual two private target streams/journals and compiled helper
    # metadata. A status label or copied finite.json alone is insufficient.
    capture_spec=importlib.util.spec_from_file_location('sleep_parent_current_capture',HERE/'capture_reference.py')
    capture=importlib.util.module_from_spec(capture_spec)
    capture_spec.loader.exec_module(capture)
    pinned=packet.load(HERE/'source-pins.json')
    helper_metadata=packet.load(HERE/'helper-pins.json')['classes']
    prepared=packet.load(HERE/'profiles.json')
    if receipt['helper_before_sha256']!=historical['executed_helper_sha256'] or \
            receipt['binary_input_sha256']!=historical['execution_binary_input_sha256']:
        raise ValueError('actual reproduced helper/JDK/SDK inventories differ')
    if receipt['input_before_sha256'].get('producer/capture_reference.py')!=\
            historical['current_reproduction_sha256']['capture_reference.py']:
        raise ValueError('historical execution cannot be rebound to current reproduction tools')
    for member in HERE.iterdir():
        if member.is_file() and member.suffix in ('.py','.java','.json','.tsv','.txt') and \
                receipt['input_before_sha256'].get('producer/'+member.name)!=packet.digest(member):
            raise ValueError('current reproduction producer input differs: '+member.name)
    for index in range(2):
        observed=capture.projection(source/f'vm-{index}',prepared,pinned,helper_metadata)
        if not packet.exact(observed,original):
            raise ValueError('actual reproduced target projection differs')
    for name,pin in receipt['helper_before_sha256'].items():
        if packet.digest(source/'classes'/name)!=pin:
            raise ValueError('actual reproduced helper bytes differ')
    for name,pin in receipt['input_before_sha256'].items():
        if name.startswith('producer/') and packet.digest(source/'producer'/name[9:])!=pin:
            raise ValueError('actual frozen current producer snapshot differs')
    for name in ('finite.json', 'projection-0.json', 'projection-1.json'):
        if not (packet.digest(source / name) == packet.FINITE and
                packet.exact(packet.load(source / name), original)):
            raise ValueError('fresh reproduction differs: ' + name)
    output.mkdir(parents=True, exist_ok=False)
    (output / 'comparison.json').write_text(json.dumps({
        'status': 'PASS_REPRODUCTION_PREFIXES_ONLY', 'profiles': 36,
        'fresh_vms': 2, 'finite_sha256': packet.FINITE,
        'current_capture_receipt_sha256': packet.digest(source / 'capture.json'),
        'historical_producer_map_rebound': False,
        'source_parent_returns': 0, 'native_qualification': False,
    }, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
