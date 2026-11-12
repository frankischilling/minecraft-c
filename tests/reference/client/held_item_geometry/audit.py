"""Audit the ordinary/read-only-JDI genuine-owner geometry capture without target writes."""
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('held_geometry_selected', HERE / 'compare.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
PCS = (335, 423, 554, 687, 824, 959)


def require(value, message):
    if not value:
        raise AssertionError(message)


def tagged(text, prefix):
    return [json.loads(line.removeprefix(prefix + ' '), object_pairs_hook=P.unique)
            for line in text.splitlines() if line.startswith(prefix + ' ')]


def audit(report, ordinary, target, events):
    expected_owners = {'app': 'kq', 'world': 'fd', 'player': 'dc', 'engine': 'ji', 'packs': 'ik',
                       'font': 'sj', 'hand': 'ra', 'image': 'iy', 'nw': 'nw'}
    for text in (ordinary, target):
        owners = tagged(text, 'HELD_GEOMETRY_OWNERS')
        require(len(owners) == 1 and all(owners[0].get(k) == v for k, v in expected_owners.items()) and
                owners[0]['canonical_app'] is True and type(owners[0]['raw_allocations']) is int and
                owners[0]['raw_allocations'] == 0, 'actual genuine constructor graph and canonical App')
        require(len(tagged(text, 'HELD_GEOMETRY_DEVICE')) == 1 and
                tagged(text, 'HELD_GEOMETRY_DEVICE')[0]['hidden_pbuffer'] is True,
                'actual hidden Pbuffer device')
        done = tagged(text, 'HELD_GEOMETRY_COMPLETE')
        require(len(done) == 1 and done[0]['profiles'] == 6 and done[0]['network'] == 0 and
                done[0]['game_substitution'] is False and done[0]['random_rebinding'] is False and
                type(done[0]['raw_allocations']) is int and done[0]['raw_allocations'] == 0,
                'six returned source calls and external-effect guards')
        inputs = tagged(text, 'HELD_GEOMETRY_INPUT')
        require([(r['profile'], r['id'], r['icon'], r['item_class'], r['count'], r['damage']) for r in inputs] ==
                [(i, item, icon, 'qz' if item == 261 else 'qd', 1, 0)
                 for i, (item, icon) in enumerate(zip((268, 272, 267, 276, 283, 261), P.ICONS))],
                'actual Player icon queries and genuine declared Stacks')
        initial = tagged(text, 'HELD_GEOMETRY_INITIAL')
        require(len(initial) == 1 and initial[0]['static'] == {'b': True, 'c': False} and
                initial[0]['all_zero'] is False and len(initial[0]['words']) == 768 and
                initial[0]['instance']['o'] == 0, 'actual Font-used initial NW footprint, never cleared')
        require(any(word != '00000000' for word in initial[0]['words']), 'stale source Font geometry retained')
        returns = tagged(text, 'HELD_GEOMETRY_RETURN')
        require([r['profile'] for r in returns] == list(range(6)) and
                all(r['gl_error'] == 0 and r['nw_after']['o'] == 0 for r in returns),
                'six actual original returns/closed NW batches/GL checks')
    for label in ('HELD_GEOMETRY_INITIAL', 'HELD_GEOMETRY_RETURN', 'HELD_GEOMETRY_DEVICE'):
        require(tagged(ordinary, label) == tagged(target, label),
                'ordinary and read-only-JDI exact observed state: ' + label)
    filtered = [e for e in events if e['event'] != 'live-code']
    require([(r['event'], r['profile']) for r in filtered] ==
            [(event, profile) for profile in range(6) for event in
             ('ra-entry', *(['nw-draw'] * 6), 'ra-return')], 'complete ordered source entry/6flush/return groups')
    entry = [e for e in events if e['event'] == 'ra-entry']
    for key in ('receiver', 'app', 'player'):
        require(len({r[key] for r in entry}) == 1, 'actual retained owner identity: ' + key)
    require(len({r['stack'] for r in entry}) == 6 and
            all(r['player-class'] == 'dc' and r['stack-class'] == 'iz' for r in entry) and
            [r['stack-id'] for r in entry] == [268, 272, 267, 276, 283, 261], 'actual Player/Stack receiver identities')
    batches = [e for e in events if e['event'] == 'nw-draw']
    require(len({r['receiver'] for r in batches}) == 1, 'actual same singleton NW throughout')
    rows = []
    for record, (icon, batch) in zip(batches, P.ORDER):
        state = record['state']
        require(record['batch'] == batch and record['profile'] == P.ICONS.index(icon) and
                state['h'] == (6 if batch < 2 else 96) and state['o'] == (48 if batch < 2 else 768) and
                state['p'] == (4 if batch < 2 else 64) and state['l'] is False and
                state['m'] is True and state['n'] is True and state['r'] == 7 and
                all(state[k] == '0000000000000000' for k in ('s', 't', 'u')),
                'actual UV/normal flags, no color, quad conversion and unchanged translation')
        require(record['source-frames'] == [{'class': 'ra', 'method': 'a',
                'descriptor': '(Lls;Liz;)V', 'pc': PCS[batch]}], 'actual source NW flush callsite')
        row = {'icon': icon, 'batch': batch, 'words': record['words']}
        require(all(word == '00000000' for i, word in enumerate(row['words']) if i % 8 in (5, 7)),
                'observed inactive color/reserved slots zero in this genuine prior state')
        rows.append(row)
    P.validate(rows)
    pins = [e for e in events if e['event'] == 'live-code']
    require([(r['class'], r['method'], r['descriptor']) for r in pins] ==
            [('ra', 'a', '(Lls;Liz;)V'), ('nw', 'a', '()V')], 'both actual entered source methods pinned')
    for pin in pins:
        actual = next(m for m in report['pins']['classes'][pin['class']]['methods']
                      if m['name'] == pin['method'] and m['descriptor'] == pin['descriptor'])
        require(actual['code_sha256'] == pin['sha256'] and actual['code_bytes'] == pin['bytes'],
                'unchanged live Code body matches static original archive')
    return {'rows': rows, 'live_code': pins, 'draw_entries': batches,
            'initial': tagged(ordinary, 'HELD_GEOMETRY_INITIAL')[0],
            'owners': tagged(ordinary, 'HELD_GEOMETRY_OWNERS')[0],
            'device': tagged(ordinary, 'HELD_GEOMETRY_DEVICE')[0],
            'words': sum(len(r['words']) for r in rows), 'source_vertices': 6 * 264,
            'packed_vertices': 6 * 396}
