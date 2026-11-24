"""Original-only integrity and cross-observation corruption controls; no JVM/assets."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location('redstone_world_packet', Path(__file__).with_name('packet.py'))
packet = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packet)


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sealed = packet.read_packet()
        cls.data = {name: packet.load_cohort(packet.HERE / name) for name in packet.COHORTS}

    def reject(self, name, mutate, message, reproject=False):
        data = copy.deepcopy(self.data[name])
        mutate(data)
        if reproject:
            data['stdout'] = ('16 achievements\r\n151 recipes\r\n' +
                              '\r\n'.join(data.get('owners', []) + data['selected']) +
                              '\r\n').encode('ascii')
        with self.assertRaisesRegex(ValueError, message):
            packet.validate_cohort(name, data)

    @staticmethod
    def change_selected(data, line, column, value):
        cells = data['selected'][line].split('\t')
        cells[column] = value
        data['selected'][line] = '\t'.join(cells)

    @staticmethod
    def event(data, kind, at):
        return next(e for e in data['events'] if e['event'] == kind and e.get('at') == at)

    def copy_packet(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        for p in packet.HERE.iterdir():
            if p.name == '__pycache__':
                continue
            if p.is_dir():
                shutil.copytree(p, root / p.name)
            else:
                shutil.copyfile(p, root / p.name)
        return root

    def test_complete_separate_cohorts(self):
        self.assertEqual([len(c['rows']) for c in self.sealed['cohorts'].values()], [56, 20, 10, 8, 6])
        self.assertEqual([c['counts']['events'] for c in self.sealed['cohorts'].values()],
                         [9312, 4412, 1908, 1729, 135])
        self.assertEqual([c['counts']['source_exceptions'] for c in self.sealed['cohorts'].values()],
                         [0, 0, 0, 0, 6])
        owners = self.sealed['cohorts']['signed']['owners']
        self.assertEqual((owners['requests'], owners['unique']), (12, 10))
        self.assertEqual([s[5] for s in owners['slots']], list(range(10)) + [0, 1])

    def test_signed_owner_closed_roster(self):
        line = self.data['signed']['owners'][0]
        for changed in ([], [line, line], [line + '\textra'],
                        [line.replace('SIGNED_OWNERS\t12\t10', 'SIGNED_OWNERS\t10\t10')],
                        [line.replace('SIGNED_OWNERS\t12\t10', 'SIGNED_OWNERS\t12\t12')],
                        [line.replace('SIGNED_OWNERS\t12\t10', 'SIGNED_OWNERS\t12\ttrue')]):
            with self.subTest(receipt=changed), self.assertRaises(ValueError):
                packet.validate_signed_owners(changed)

    def test_signed_owner_actual_alias_coordinates_and_identity(self):
        def alter(slot, column, value):
            def mutate(data):
                fields = data['owners'][0].split('\t')
                values = fields[3 + slot].split(',')
                values[column] = str(value)
                fields[3 + slot] = ','.join(values)
                data['owners'][0] = '\t'.join(fields)
            return mutate
        for slot, column, value in ((10, 3, 1048576), (10, 5, 10), (11, 5, 0),
                                    (11, 4, 0), (9, 1, 1048576), (0, 0, 1), (1, 5, 0)):
            with self.subTest(slot=slot, column=column, value=value):
                self.reject('signed', alter(slot, column, value), 'signed requests|owner identity', True)

    def test_signed_owner_raw_receipt_position_and_join(self):
        raw = (self.data['signed']['owners'][0] + '\r\n').encode('ascii')
        for change in (lambda b: b.replace(raw, b'', 1),
                       lambda b: b.replace(raw, raw + raw, 1),
                       lambda b: b.replace(raw, b'', 1) + raw,
                       lambda b: b.replace(raw, raw.replace(b'\t12\t10', b'\t12\t12'), 1)):
            with self.subTest(change=change):
                self.reject('signed', lambda d: d.update(stdout=change(d['stdout'])), 'full exact raw stdout')

    def test_signed_owner_original_key_and_get_pins(self):
        for owner, method, desc in (('yy', 'a', '(II)I'), ('ok', 'b', '(II)Llm;')):
            def mutate(d):
                next(m for m in d['pins']['classes'][owner]['methods']
                     if m['name'] == method and m['descriptor'] == desc)['code_sha256'] = '0' * 64
            with self.subTest(method=owner + '.' + method):
                self.reject('signed', mutate, 'owner key/GET Code pins')

    def test_signed_owner_static_field_and_constructor_joins(self):
        mutations = (lambda c: c['lm_constructor']['x'].update(field='k'),
                     lambda c: c['lm_constructor']['z'].update(descriptor='J'),
                     lambda c: c['lm_constructor']['x'].update(argument_local=3),
                     lambda c: c['lm_constructor']['z'].update(load_pc=54),
                     lambda c: c['lm_constructor']['x'].update(store_pc=56),
                     lambda c: c['lm_constructor'].update(code_sha256='0' * 64),
                     lambda c: c['packed_key'].update(code_bytes=38),
                     lambda c: c['cached_lookup'].update(method='c'),
                     lambda c: c.update(owner_aliases='twelve distinct owners'))
        for mutate in mutations:
            with self.subTest(mutation=mutate):
                self.reject('signed', lambda d: mutate(d['owner_contract']), 'LM coordinate|owner')
        def mutate_pin(d):
            constructor = next(m for m in d['pins']['classes']['lm']['methods']
                               if m['name'] == '<init>' and m['descriptor'] == '(Lfd;II)V')
            constructor['code_sha256'] = '0' * 64
        self.reject('signed', mutate_pin, 'LM coordinate constructor Code pin')

    def test_duplicate_json_keys(self):
        for text in ('{"profiles":6,"profiles":6}', '{"outer":{"seq":1,"seq":1}}'):
            with self.subTest(text=text), self.assertRaisesRegex(ValueError, 'duplicate JSON'):
                packet.loads(text)

    def test_canonical_profile_transport(self):
        for raw in (b'name 0\r\n', b'name 0', b'name\xff 0\n'):
            with self.subTest(raw=raw), self.assertRaises((ValueError, UnicodeError)):
                packet.lf(raw)
        for line in ('bad  0 ' + ' '.join(['0'] * 12), 'bad ' + ' '.join(['0'] * 12),
                     'bad ' + ' '.join(['2147483648'] * 13), 'bad ' + ' '.join(['-0'] * 13)):
            with self.subTest(line=line), self.assertRaises(ValueError):
                packet.profiles([line])

    def test_raw_stdout_is_closed(self):
        for mutation in (lambda b: b.replace(b'16 achievements', b'17 achievements', 1),
                         lambda b: b[len(b'16 achievements\r\n'):],
                         lambda b: b'ignored line\r\n' + b,
                         lambda b: b.replace(b'\r\n', b'\n'),
                         lambda b: b + b'extra\r\n'):
            with self.subTest(mutation=mutation):
                self.reject('flat', lambda d: d.update(stdout=mutation(d['stdout'])), 'full exact raw stdout')

    def test_declared_count_and_type(self):
        for value in (True, 55, 57):
            with self.subTest(value=value):
                self.reject('flat', lambda d: d['declared'].update(profiles=value), 'declared input count')

    def test_input_alias_and_wrapped_texture(self):
        self.reject('flat', lambda d: self.change_selected(d, 1, 6, '165'), 'literal bm', True)
        self.reject('signed', lambda d: self.change_selected(d, 1, 15, '-16777216'), 'actual EW minimum', True)
        self.reject('nullable', lambda d: self.change_selected(d, 1, 11, '0'), 'nullable operands', True)

    def test_closed_effect_and_primitive_keys(self):
        self.reject('flat', lambda d: self.change_selected(d, 2, 12,
                    d['selected'][2].split('\t')[12] + ',foreign=false'), 'complete CV primitive', True)
        self.reject('flat', lambda d: self.change_selected(d, 2, 13,
                    d['selected'][2].split('\t')[13] + ',foreign=0'), 'complete selected NW', True)
        self.reject('flat', lambda d: self.change_selected(d, 2, 11,
                    ','.join(['0000000000000000'] * 6)), 'raw Block bounds', True)

    def test_cursor_and_own_prior_hash_continuity(self):
        self.reject('flat', lambda d: self.change_selected(d, 5, 6, '0'), 'continuous returned packed cursor', True)
        self.reject('flat', lambda d: self.change_selected(d, 3, 3, '0'), 'Arrays.hashCode', True)
        self.reject('flat', lambda d: self.change_selected(d, 3, 2, '2097152'), 'own-prior/reference', True)
        def corrupt_word(d):
            values = d['selected'][2].split('\t')[15].split(',')
            values[0] = '00000000'
            self.change_selected(d, 2, 15, ','.join(values))
        self.reject('flat', corrupt_word, 'Arrays.hashCode', True)

    def test_inactive_tail_and_real_buffer_classes(self):
        def corrupt_tail(d):
            values = d['selected'][2].split('\t')[15].split(',')
            values[-1] = '00000001'
            self.change_selected(d, 2, 15, ','.join(values))
        self.reject('flat', corrupt_tail, 'inactive tail', True)
        self.reject('flat', lambda d: self.change_selected(d, 3, 5,
                    d['selected'][3].split('\t')[5].replace('DirectIntBufferU', 'HeapIntBuffer')),
                    'reference/buffer', True)

    def test_original_denial_receipts(self):
        self.reject('signed', lambda d: d['selected'].__setitem__(-3,
                    d['selected'][-3].replace('world-writes=6', 'world-writes=3')), 'denial receipts', True)
        self.reject('flat', lambda d: d['selected'].__setitem__(-2,
                    d['selected'][-2].replace('network=2', 'network=0')), 'denial receipts', True)

    def test_live_code_and_method_identity(self):
        self.reject('flat', lambda d: d['live'].__setitem__(0, d['live'][0][:-1] + '0'), 'Code joins archive')
        self.reject('flat', lambda d: next(m for m in d['pins']['classes']['cv']['methods']
                    if m['name'] == 'g' and m['descriptor'] == '(Luu;III)Z').update(code_sha256='0' * 64),
                    'Code joins archive|CV.g Code')
        self.reject('flat', lambda d: d['live'].append(d['live'][0]), 'Code joins archive')

    def test_sequence_typed_call_and_stack(self):
        self.reject('flat', lambda d: d['events'][1].update(seq=9), 'event sequence')
        self.reject('flat', lambda d: self.event(d, 'return', 'ew.e(III)I@57').update(call=True), 'nested return')
        self.reject('flat', lambda d: self.event(d, 'entry', 'ew.e(III)I@0').update(caller='uu.d(Lxp;III)F@13'),
                    'caller/entry stack')
        self.reject('flat', lambda d: self.event(d, 'return', 'nw.a(DD)V@15').update(at='nw.a(DD)V@0'),
                    'return instruction')

    def test_genuine_cache_and_singleton_identities(self):
        self.reject('flat', lambda d: self.event(d, 'entry', 'ew.e(III)I@0').update(receiver='ref:observedBlock'),
                    'EW/query')
        self.reject('flat', lambda d: self.event(d, 'entry', 'nw.a(FFF)V@0').update(receiver='ref:observedCache'),
                    'NW singleton')
        self.reject('flat', lambda d: next(e for e in d['events'] if e.get('field') == 'uu.m').update(
                    value='ref:observedEmissions'), 'static occurrence')
        self.reject('capability', lambda d: next(e for e in d['events'] if e.get('field') == 'uu.bi').update(
                    value='ref:observedRepeaterOn'), 'static occurrence')

    def test_saved_cells_and_source_query_operands(self):
        self.reject('flat', lambda d: self.event(d, 'entry', 'ew.e(III)I@0')['args'].__setitem__(1, 65),
                    'fresh CV query operands')
        self.reject('vertical', lambda d: self.event(d, 'entry', 'sm.e(Lxp;IIII)Z@0')['args'].__setitem__(2, 63),
                    'connectivity operands')
        self.reject('flat', lambda d: self.event(d, 'return', 'ew.a(III)I@103').update(result=1),
                    'saved-cell operands')

    def test_full_state_and_color_effects(self):
        self.reject('flat', lambda d: d['events'][0]['nw'].update(extra=0), 'primitive keys')
        self.reject('flat', lambda d: d['events'][0]['nw'].update(w=1), 'typed NW')
        self.reject('flat', lambda d: self.event(d, 'return', 'nw.a(FFF)V@19')['nw'].update(k=0),
                    'effect transition')
        self.reject('flat', lambda d: self.event(d, 'entry', 'nw.a(FFF)V@0')['args'].__setitem__(0, '3f800000'),
                    'effect transition')

    def test_uv_position_forwarding_and_writer_count(self):
        self.reject('flat', lambda d: self.event(d, 'entry', 'nw.a(DD)V@0')['args'].__setitem__(0, '0' * 16),
                    'UV/position forwarding')
        self.reject('flat', lambda d: self.event(d, 'return', 'nw.a(DDD)V@450')['nw'].update(o=16),
                    'effect transition')

    def test_observed_positions_join_actual_packed_words(self):
        def mutate(d):
            parent = self.event(d, 'entry', 'nw.a(DDDDD)V@0')
            parent['args'][0] = struct.pack('>d', 123.0).hex()
            child = next(e for e in d['events'] if e['event'] == 'entry' and
                         e['at'] == 'nw.a(DDD)V@0' and e['call'] > parent['call'])
            child['args'][0] = parent['args'][0]
        self.reject('flat', mutate, 'append words join literal effects')

    def test_source_vertex_group_order(self):
        self.reject('flat', lambda d: self.event(d, 'entry', 'nw.a(DDDDD)V@0').update(
                    caller='cv.g(Luu;III)Z@922'), 'vertex-PC group ordering')

    def test_nullable_first_throw_and_suspended_callers(self):
        self.reject('nullable', lambda d: self.event(d, 'exception', 'cv.g(Luu;III)Z@13').update(
                    type='java.lang.ArrayIndexOutOfBoundsException'), 'throw/catch/frame')
        self.reject('nullable', lambda d: self.event(d, 'exception', 'ew.h(III)Z@25')['frames'].__setitem__(1,
                    'cv.g(Luu;III)Z@462'), 'throw/catch/frame')
        self.reject('nullable', lambda d: self.event(d, 'exception', 'cv.g(Luu;III)Z@13').update(
                    catch='RedstoneObservation.main([Ljava/lang/String;)V@1248'), 'throw/catch/frame')

    def test_provenance_scope_counts_and_raw_receipts(self):
        name = 'flat'
        provenance = packet.loads((packet.HERE / name / 'provenance.json').read_text(encoding='utf-8'))
        files = self.sealed['manifest']['cohorts'][name]['files_sha256']
        for mutate in (lambda p: p.update(original_inputs_unchanged=False),
                       lambda p: p.update(source_writes_from_jdi=True),
                       lambda p: p.update(queries=p['queries'] - 1),
                       lambda p: p['runs'][0]['capture'].update(timed_out=True),
                       lambda p: p['runs'][0]['world_after_sha256'].update({'level.dat': '0' * 64}),
                       lambda p: p.update(original_stdout_sha256=['0' * 64] * 3)):
            candidate = copy.deepcopy(provenance)
            mutate(candidate)
            with self.subTest(mutation=mutate), self.assertRaises(ValueError):
                packet.validate_provenance(name, candidate, self.data[name],
                                           self.sealed['cohorts'][name], files)

    def test_immutable_seal_rejects_file_and_repaired_manifest(self):
        root = self.copy_packet()
        path = root / 'flat' / 'profiles.txt'
        path.write_bytes(path.read_bytes() + b'foreign\n')
        with self.assertRaisesRegex(ValueError, 'immutable cohort file'):
            packet.read_packet(root)
        manifest = packet.loads((root / 'manifest.json').read_text(encoding='utf-8'))
        manifest['cohorts']['flat']['files_sha256']['profiles.txt'] = packet.digest(path)
        (root / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'sealed packet manifest'):
            packet.read_packet(root)


if __name__ == '__main__':
    unittest.main()
