"""Native Lightning contracts and optional independent original prefix comparison."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile


MASK = (1 << 48) - 1
MULT = 0x5deece66d
COUNT = 5
DIFFICULTY = [0, 1, 2, 2, 2]
Y = [64.5, -1.5, 140.0, -20.0, 64.5]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical(value):
    if isinstance(value, dict):
        if 'class' in value:
            return {'nonnull': True}
        return {key: canonical(item) for key, item in value.items()}
    if isinstance(value, list):
        return [canonical(item) for item in value]
    return value


def advance(seed):
    return (seed * MULT + 11) & MASK


def replay(seed):
    first = advance(seed)
    after_long = advance(first)
    low = after_long >> 16
    signed_low = low if low < 1 << 31 else low - (1 << 32)
    word = (((first >> 16) << 32) + signed_low) & ((1 << 64) - 1)
    after = after_long
    while True:
        after = advance(after)
        bits = after >> 17
        repeat = bits % 3
        if bits - repeat + 2 < 1 << 31:
            return {'private_seed_before': f'{seed:012x}', 'next_long_word': f'{word:016x}',
                    'private_seed_after_long': f'{after_long:012x}', 'next_int3': repeat,
                    'private_seed_after': f'{after:012x}'}


def dword(value):
    return {'D': struct.pack('>d', value).hex()}


def native_contracts(rows, seeds):
    require(len(rows) == COUNT, 'five actual native constructor rows')
    u = (8682522807148012 * 1181783497276652981) & ((1 << 64) - 1)
    seed = (u ^ (-987654321 & ((1 << 64) - 1)) ^ MULT) & MASK
    own = replay(seed)
    for index, row in enumerate(rows):
        require((row['event'], row['case'], row['scope'], row['entropy']) ==
                ('native-constructor', index, 'genuine_RAW_GAME_World_prefix', 'fresh_declared_clock'),
                'actual constructor and native context labels')
        require(row['difficulty'] == DIFFICULTY[index] and row['null_world'] == (index == 4) and
                row['ready'] == (index != 4), 'actual case inputs and terminal phase')
        require(row['entity_counter_after'] == row['entity_counter_before'] + 1 and
                row['fields']['sn.aD'] == row['entity_counter_before'], 'relative actual Entity ID consumption')
        require(row['math_holder_cold'] and row['math_draws'] == 0 and row['own_random_calls'] == 2,
                'two own calls and no native Math initialization/draw')
        require(row['source_exception'] == (index == 4) and
                row['exception_pc'] == (52 if index == 4 else 0) and
                row['constructor_pc'] == (48 if index == 4 else 262), 'actual NULL52 failed prefix')
        fields = row['fields']
        require(len(fields) == 69 and fields['c.b'] == 2 and
                fields['c.a'] == {'J': own['next_long_word']} and fields['c.c'] == own['next_int3'] + 1,
                'fresh declared-clock actual own RNG fields')
        require(row['private_random'] == {'seed48': own['private_seed_after'], 'cached': False,
                                         'gaussian': '0000000000000000'}, 'actual own Random final state/cache')
        for keys, value in [(('sn.aM', 'sn.aJ', 'sn.bl'), -17.25),
                            (('sn.aN', 'sn.aK', 'sn.bm'), Y[index]),
                            (('sn.aO', 'sn.aL', 'sn.bn'), 31.75)]:
            require(all(fields[key] == dword(value) for key in keys), 'actual pose/current/previous coordinates')
        require(fields['sn.aI'] == (None if index == 4 else {'nonnull': True}) and
                fields['sn.aF'] is False, 'actual SN-only World/actor fields')
        require(row['watcher'] == {'size': 1, 'dirty': False, 'records':
                                  [{'index': 0, 'type': 0, 'value': 0, 'dirty': True}]},
                'actual typed early Byte0 with empty Lightning hook')
        scalar = replay(seeds[index])
        require(row['scalar_replay'] == {'label': 'separate_preseed_scalar_replay', **scalar},
                'separately labelled local scalar replay')


def compare_original(rows, original):
    normal, debug, profiles = (original[key] for key in ('normal_rows', 'debugger_rows', 'entropy_profiles'))
    require(len(normal) == len(debug) == len(profiles) == COUNT, 'five paired original constructor cases')
    require(original['source_exceptions'] == [{'event': 'source-exception', 'case': 4,
            'exception': 'java.lang.NullPointerException', 'class': 'c', 'method': '<init>',
            'descriptor': '(Lfd;DDD)V', 'pc': 52}], 'read-only original actual NULL exception event')
    direct = scalar_checks = null_fields = 0
    for index, (native, ordinary, observed, profile) in enumerate(zip(rows, normal, debug, profiles)):
        for source in (ordinary, observed):
            require(source['case'] == index and source['difficulty'] == DIFFICULTY[index] and
                    source['null_world'] == (index == 4), 'original case identity/input context')
            require(source['math_seed_before'] == source['math_seed_after'] and
                    source['entity_counter_after'] == source['entity_counter_before'] + 1,
                    'independent original no-Math and ID evidence')
            require(source['exception'] == ('java.lang.NullPointerException' if index == 4 else None),
                    'original source result')
        require(((int(profile['u'], 16) ^ int(profile['nano'], 16) ^ MULT) & MASK) ==
                int(profile['private_seed_before'], 16), 'observed actual clock-to-private-seed transform')
        scalar = native['scalar_replay']
        for key in ('private_seed_before', 'private_seed_after_long', 'private_seed_after',
                    'next_long_word', 'next_int3'):
            require(scalar[key] == profile[key], 'separate original scalar profile ' + key)
            scalar_checks += 1
        if index == 4:
            require(ordinary['fields'] is observed['fields'] is None,
                    'no ordinary failed object is fabricated')
            candidates = [p for p in original['checkpoints'] if p['case'] == 4 and
                          p['location'] == {'class': 'c', 'method': '<init>',
                                            'descriptor': '(Lfd;DDD)V', 'pc': 52}]
            require(len(candidates) == 1, 'one actual failed object prefix from read-only JDI C52')
            sources = candidates
        else:
            sources = [ordinary, observed]
        for source in sources:
            fields = canonical(source['fields'])
            require(set(fields) == set(native['fields']), 'complete actual SN/C field shape')
            for key in fields.keys() - {'sn.aD', 'c.a', 'c.c'}:
                require(native['fields'][key] == fields[key], f'independent deterministic case {index} field {key}')
                direct += 1
                null_fields += index == 4
            require(native['bounds'] == source['bounds'] and native['watcher'] == source['watcher'],
                    'independent original pose bounds and typed watcher')
        actual = sources[-1]['fields']
        require(actual['c.a'] == {'J': scalar['next_long_word']} and
                actual['c.c'] == scalar['next_int3'] + 1, 'separate replay matches original random-derived fields')
        scalar_checks += 2
    return {'direct_independent_field_comparisons': direct,
            'jdi_only_failed_object_field_comparisons': null_fields,
            'separately_labelled_scalar_checks': scalar_checks,
            'original_checkpoint_count': len(original['checkpoints']),
            'scope': 'Original calls use a fully constructed prepared World; native calls use genuine RAW GAME World prefixes. Only reached constructor fields, pose, watcher, relative ID consumption and zero Math draws are compared directly. Native declared-clock entropy and original actual entropy remain independent. Observed own pre-seeds drive a separate plain local Random scalar replay; no Entity or profile is rebound after World setup. The failed original object is observed only through read-only JDI C52.'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--observations', type=Path)
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='lightning-construction-', dir=args.work_dir.resolve()))
    original = json.loads(args.observations.read_text()) if args.observations else None
    seeds = ([int(profile['private_seed_before'], 16) for profile in original['entropy_profiles']]
             if original is not None else [0, 1, 0x123456789abc, 0xffffffffffff, 0x5deece66d])
    require(len(seeds) == COUNT and all(0 <= seed <= MASK for seed in seeds), 'five bounded scalar inputs')
    tape = out / 'scalar-inputs.txt'
    tape.write_text(''.join(f'{seed:012x}\n' for seed in seeds), encoding='ascii', newline='\n')
    executable = args.executable.resolve(strict=True)
    inputs = [executable, Path(__file__).resolve(), tape]
    if args.observations:
        inputs.append(args.observations.resolve(strict=True))
    before = {str(path): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before, 'original_comparison': None}
    try:
        actual = out / 'native.ndjson'
        command = [str(executable), '--probe', str(tape), str(actual)]
        result = subprocess.run(command, cwd=out, stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=110)
        (out / 'native.stdout').write_bytes(result.stdout)
        (out / 'native.stderr').write_bytes(result.stderr)
        report['command'] = command
        footer = re.fullmatch(rb'Lightning construction: ([1-9][0-9]*) checks; source prefixes, own RNG and terminal ownership\r?\n', result.stdout)
        require(result.returncode == 0 and not result.stderr and footer is not None,
                f'native Lightning contracts failed; inspect {out}')
        require(actual.stat().st_size < 1048576, 'bounded native transport')
        rows = [json.loads(line) for line in actual.read_text().splitlines()]
        native_contracts(rows, seeds)
        if original is not None:
            report['original_comparison'] = compare_original(rows, original)
        report.update(status='PASS', checks=int(footer.group(1)), constructors=COUNT,
                      native_result=str(actual), scalar_inputs='observed original private pre-seeds' if original is not None else 'authored scalar pre-seeds',
                      scope='Bare native contracts remain mandatory and Java-independent. Optional probe executes five genuine native C constructors in declared RAW GAME World scopes, checks fresh declared-clock own RNG/no-Math and transports deterministic fields. Separate local scalar replay never substitutes constructor entropy.')
        print(f'Lightning constructors: {report["checks"]} checks; {"independent original comparison" if original is not None else "authored native contracts"}; {out / "run.json"}')
    finally:
        report['input_after_sha256'] = {str(path): digest(path) for path in inputs}
        unchanged = before == report['input_after_sha256']
        if not unchanged:
            report['status'] = 'FAIL'
        (out / 'run.json').write_text(json.dumps(report, indent=2) + '\n', newline='\n')
        require(unchanged, 'native probe inputs changed')


if __name__ == '__main__':
    main()
