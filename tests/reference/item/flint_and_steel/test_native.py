"""Reject corrupted Flint and Steel effects, source prefixes, traversal and cleanup transport."""
import copy
import hashlib
import json
import unittest

import run_native as N

W, P = N.W, N.P


def runs(data):
    result = []
    start = 0
    while start < len(data):
        end = start + 1
        while end < len(data) and data[end] == data[start]: end += 1
        if data[start]: result.append([start, end - start, data[start]])
        start = end
    return result


def raw_state(profile, source, phase, after):
    value = N.selected_state(source)
    value['phase'] = phase
    arrays = W.plane_arrays(profile, after)
    P.require(type(arrays) is list and len(arrays) == len(value['chunk_planes']), 'independent complete Flint and Steel plane model')
    for target, chunk in zip(value['chunk_planes'], arrays):
        P.require(P.exact(target['xyz'], chunk['xyz']) and set(chunk['planes']) == set(N.N.PLANES), 'independent Chunk coordinates')
        for name, data in chunk['planes'].items():
            P.require(len(data) == N.N.PLANES[name] and hashlib.sha256(data).hexdigest() == target['planes'][name],
                      'independently derived Flint and Steel plane matches actual original')
        target['planes'] = {name: runs(data) for name, data in chunk['planes'].items()}
    return value


def fixture(mode,originals):
    profile=N.profile_for(mode);original=originals[mode]
    expected=W.model(profile,original['prepared_class']);facts=expected['progress']
    exception=N.exception_class(original);normal=exception is None;boundary=expected['invocation_null']
    origin=expected['exception_origin']
    provider=origin is not None and P.exact([origin['class'],origin['method'],origin['pc']],['fd','c',6])
    empty_site=dict(**{'class':None},method=None,pc=0)
    child=dict(**{'class':'fd'},method='c',pc=6) if provider else copy.deepcopy(empty_site)
    item=None
    if profile['operation']!='ob' or facts['item_entered']:
        if boundary:pc=1;owner='iz'
        elif profile['operation']=='direct' or (facts['ye_entered'] and not facts['wrapper_body_returned']):pc=facts['ye_pc'];owner='ye'
        else:pc=facts['wrapper_pc'];owner='iz'
        actual_site=dict(**{'class':owner},method='a',pc=pc)
        def world_view(key):
            if not facts[key+'_entered']:return None
            if facts[key+'_returned']:return dict(status=0,access_fault=0,access_pc=0)
            if exception=='java/lang/IllegalStateException':
                return dict(status=3,access_fault=0,access_pc={'sound':41,'cell':34,'range':40}[original['events'][-1]['kind']])
            return dict(status=5,access_fault=1,access_pc=9)
        item=dict(pc=pc,target_xyz=W.D.shifted(profile['xyz'],profile['face']) if facts['ye_entered'] else copy.deepcopy(profile['xyz']),
            item_id=259 if facts['ye_entered'] else None,returned=normal,
            body_result=facts['durability_returned'] if profile['operation']=='direct' else facts['wrapper_body_returned'],
            sound_entered=facts['sound_entered'],sound_returned=facts['sound_returned'],setter_entered=facts['setter_entered'],
            setter_returned=facts['setter_returned'],setter_value=facts['setter_value'],durability_entered=facts['durability_entered'],
            durability_returned=facts['durability_returned'],used_stat_returned=facts['use_stat_returned'],query_provider_null=provider,
            sound=world_view('sound'),setter=world_view('setter'),site=actual_site,child=copy.deepcopy(child))
    ob=None
    if profile['operation']=='ob':
        ob={key:facts[key] for key in ('activation_entered','activation_returned','activated','item_entered','item_returned')}
        ob.update(pc=facts['ob_pc'],zero_slot_cleared=False,selected_slot_stored=False)
        actual_site=dict(**{'class':'ob'},method='a',pc=facts['ob_pc'])
    else:actual_site=copy.deepcopy(item['site'])
    outcome=dict(mode=mode,input=copy.deepcopy(profile),prepared_class=copy.deepcopy(original['prepared_class']),raw=0 if normal else 9,
        returned=normal,result=original['result'],exception=exception,native_policy=False,source_exception=not normal,source_error=False,
        query_provider_null=provider,site=actual_site,child=child,progress=dict(item=item,ob=ob))
    rows=[('FLINT_STATE ',raw_state(profile,original['before'],'before',False))]
    rows += [('FLINT_EVENT ',{k:copy.deepcopy(v) for k,v in event.items() if k!='stack_trace'}) for event in original['events']]
    rows.append(('FLINT_OUTCOME ',outcome))
    after=raw_state(profile,original['after'],'after',True)
    for phase in ('after','retired','cleanup'):
        state=copy.deepcopy(after)
        state['phase']=phase
        rows.append(('FLINT_STATE ',state))
        if phase=='retired' and N.receipt_expected(profile):
            rows.append(('FLINT_RECEIPT ',dict(busy_before=9,before_holds=2,after_busy_holds=2,after_call_holds=1,retire=0,
                same_identity=True,selected_source=True,exception=exception,attempts=2)))
    return rows


def text(rows,mode):return '\n'.join(prefix+json.dumps(value,separators=(',',':')) for prefix,value in rows)+'\nPASS: 101 Flint and Steel checks;mode='+mode+'\n'
def states(rows,after=False):
    result=[value for prefix,value in rows if prefix=='FLINT_STATE ']
    return result[1:] if after else result
def events(rows):return [value for prefix,value in rows if prefix=='FLINT_EVENT ']
def outcome(rows):return next(value for prefix,value in rows if prefix=='FLINT_OUTCOME ')
def lifetime(rows):return next(value for prefix,value in rows if prefix=='FLINT_RECEIPT ')

def update_after(rows,action):
    for value in states(rows,True):action(value)


class NativeReader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.originals={row['mode']:row for row in W.read_reference()['rows']}
        cls.fixture_wire={}

    def fresh_fixture(self, mode):
        if mode not in self.fixture_wire:
            rows = fixture(mode, self.originals)
            self.fixture_wire[mode] = json.dumps(rows, separators=(',', ':')).encode('utf-8')
        return [(prefix, value) for prefix, value in json.loads(self.fixture_wire[mode])]

    def check(self,rows,mode):return N.validate(N.parse(text(rows,mode),mode),mode,self.originals)
    def rejected(self,mode,action):
        rows=self.fresh_fixture(mode);action(rows)
        with self.assertRaises(ValueError):self.check(rows,mode)

    def test_all_sealed_pairs_and_canonical_arguments(self):
        self.assertEqual(set(N.MODES),set(self.originals))
        for mode in N.MODES:
            with self.subTest(mode=mode):
                self.assertTrue(self.check(self.fresh_fixture(mode),mode)['matched'])
                self.assertEqual(len(N.command('native.exe',N.profile_for(mode),N.HERE/'world',self.originals[mode]['prepared_class'])),40)

    def test_closed_protocol_duplicate_json_footer_and_exit(self):
        mode=N.MODES[0];value=text(self.fresh_fixture(mode),mode)
        for bad in (value.rstrip('\n'),value+'noise\n',value+value,value.replace('PASS: 101','PASS: 0'),
                    value.replace('FLINT_OUTCOME {','FLINT_OUTCOME {"mode":"duplicate",',1),value.replace('FLINT_STATE ','UNKNOWN ',1)):
            with self.subTest(case=bad[-100:]),self.assertRaises(ValueError):N.parse(bad,mode)
        capture=N.module('flint_exit_controls',N.ROOT/'tests/reference/client/controller/capture.py')
        valid=dict(reader_errors={},exit=0,timed_out=False,output_overflow=False,pipe_capture_complete=True,stderr_bytes=0)
        capture.require_success(valid,empty_stderr=True)
        for key,v in (('exit',1),('stderr_bytes',1),('timed_out',True),('output_overflow',True),('pipe_capture_complete',False),('reader_errors',{'stdout':'decode'})):
            with self.subTest(key=key),self.assertRaises(AssertionError):capture.require_success(dict(valid,**{key:v}),empty_stderr=True)

    def test_exact_inputs_prepared_maps_classes_and_categories(self):
        mode=N.MODES[0]
        mutations=(lambda r:outcome(r).update(raw=False),lambda r:outcome(r).update(raw=-1),
            lambda r:outcome(r)['input'].update(null_stack=0),lambda r:outcome(r)['input']['xyz'].__setitem__(0,14),
            lambda r:outcome(r)['prepared_class']['entity'].update(next_id=99),
            lambda r:outcome(r)['prepared_class']['scheduled'].update(sequence='ffffffffffffffff'),
            lambda r:outcome(r)['prepared_class']['stats'].update(dirty=True),lambda r:outcome(r).update(source_exception=True),
            lambda r:outcome(r).update(unparsed='extra'))
        for i,action in enumerate(mutations):
            with self.subTest(case=i):self.rejected(mode,action)

    def test_complete_prepared_and_changed_planes(self):
        mode='flint-wrapper'
        first = self.fresh_fixture(mode)
        original = text(first, mode)
        states(first)[0]['chunk_planes'].clear()
        self.assertEqual(text(self.fresh_fixture(mode), mode), original)
        for after in (False,True):
            for plane in N.N.PLANES:
                def change(rows,after=after,plane=plane):
                    data=bytearray(W.plane_arrays(N.profile_for(mode),after)[0]['planes'][plane]);data[-1]^=123
                    for state in states(rows,True) if after else states(rows)[:1]:state['chunk_planes'][0]['planes'][plane]=runs(data)
                with self.subTest(after=after,plane=plane):self.rejected(mode,change)
        self.rejected(mode,lambda r:states(r)[1]['chunk_planes'].pop())
        self.rejected(mode,lambda r:states(r)[-1]['stack'].update(damage=99))

    def test_callback_values_order_stack_and_saved_random_owner(self):
        mode='flint-sound-target-stone'
        def reverse(rows):
            indices=[i for i,(prefix,_) in enumerate(rows) if prefix=='FLINT_EVENT ']
            old=[rows[i] for i in indices]
            for i,value in zip(indices,reversed(old)):rows[i]=value
        for action in (reverse,lambda r:events(r)[0].update(name='step.stone'),
            lambda r:events(r)[0].update(pitch='00000000'),
            lambda r:events(r)[0]['stack'].update(damage=64),
            lambda r:r.pop(next(i for i,(p,_) in enumerate(r) if p=='FLINT_EVENT '))):self.rejected(mode,action)
        mode='flint-sound-item-random-world'
        self.rejected(mode,lambda r:update_after(r,lambda s:s['prepared_item_random'].__setitem__(0,'0000000000000000')))
        self.rejected(mode,lambda r:update_after(r,lambda s:s.update(item_random_alias_world=False)))
        mode='flint-initial-item-random-world'
        self.rejected(mode,lambda r:states(r)[0].update(item_random_alias_world=False))
        self.rejected(mode,lambda r:events(r)[0]['prepared_item_random'].__setitem__(0,'0000000000000000'))

    def test_actual_stack_durability_statistic_maps_and_no_ob_clear(self):
        modes=('flint-break64','flint-zero-count-break','flint-negative-count-break','flint-two-count-break','flint-damage-overflow')
        for mode in modes:
            self.rejected(mode,lambda r:update_after(r,lambda s:s['stack'].update(damage=65)))
        mode='flint-wrapper-break64'
        self.rejected(mode,lambda r:update_after(r,lambda s:s['stats'].update(total=[])))
        self.rejected(mode,lambda r:update_after(r,lambda s:s['stats']['unsent'][0].__setitem__(1,999)))
        self.rejected(mode,lambda r:update_after(r,lambda s:s['stack'].update(animation=0)))
        mode='flint-ob-break64'
        self.rejected(mode,lambda r:outcome(r)['progress']['ob'].update(zero_slot_cleared=True))
        self.rejected(mode,lambda r:outcome(r)['progress']['ob'].update(selected_slot_stored=True))
        self.rejected('flint-sound-stack-stick',lambda r:update_after(r,lambda s:s['stack'].update(id=259)))

    def test_world_flags_queues_lighting_and_unknown_cells(self):
        mode='flint-wrapper'
        self.rejected(mode,lambda r:update_after(r,lambda s:s['scheduled']['ordered'].clear()))
        self.rejected(mode,lambda r:update_after(r,lambda s:s['lighting']['regions'].clear()))
        self.rejected(mode,lambda r:update_after(r,lambda s:s['players'][0]['position'].__setitem__(0,'0000000000000000')))
        mode='flint-portal-cell-error-third'
        self.rejected(mode,lambda r:update_after(r,lambda s:s.update(world_o=False)))
        mode='flint-provider-bound-x-in'
        self.assertIsNone(self.originals[mode]['before']['clicked'])
        self.rejected(mode,lambda r:states(r)[0].update(clicked=[0,0]))
        self.rejected('flint-wrapper-provider-null',lambda r:outcome(r).update(query_provider_null=False))
        self.rejected('flint-ob-provider-null',lambda r:outcome(r)['child'].update(pc=0))

    def test_source_progress_and_ordinary_false_setter_continuation(self):
        mode='flint-sound-fire-id-zero'
        self.assertFalse(W.model(N.profile_for(mode),self.originals[mode]['prepared_class'])['progress']['setter_value'])
        self.rejected(mode,lambda r:outcome(r)['progress']['item'].update(setter_value=True))
        self.rejected(mode,lambda r:outcome(r).update(result=False))
        self.rejected(mode,lambda r:outcome(r)['progress']['item'].update(durability_returned=False))
        self.rejected('flint-direct-occupied',lambda r:outcome(r)['progress']['item'].update(sound_entered=True))
        self.rejected('flint-wrapper-sound-error',lambda r:outcome(r)['progress']['item']['sound'].update(access_pc=9))
        self.rejected('flint-wrapper',lambda r:outcome(r)['progress']['item'].update(used_stat_returned=False))
        self.rejected('flint-sound-target-stone',lambda r:outcome(r)['progress']['item'].update(setter_returned=False))

    def test_only_true_null_invocation_boundary_waives_original_pc(self):
        mode='flint-wrapper-stack-null'
        self.assertTrue(self.check(self.fresh_fixture(mode),mode)['invocation_boundary_scope'])
        self.assertFalse(self.check(self.fresh_fixture('flint-direct-stack-null'),'flint-direct-stack-null')['invocation_boundary_scope'])
        self.assertFalse(self.originals['flint-ob-stack-null']['result'])
        self.rejected('flint-direct-stack-null',lambda r:outcome(r)['progress']['item'].update(pc=1))
        for mode in ('flint-fire-null','flint-wrapper-provider-null','flint-ob-provider-null'):
            self.rejected(mode,lambda r:outcome(r)['site'].update(**{'class':None,'method':None,'pc':1}))
            self.rejected(mode,lambda r:outcome(r).update(invocation_null=True))

    def test_reached_receipts_unreached_ordinal_and_native_frontier(self):
        modes=[m for m in N.MODES if N.receipt_expected(N.profile_for(m))]
        self.assertEqual(len(modes),7)
        for mode in modes:
            self.rejected(mode,lambda r:r.__setitem__(slice(None),[entry for entry in r if entry[0]!='FLINT_RECEIPT ']))
        mode=modes[0]
        for key,v in (('busy_before',0),('before_holds',1),('after_busy_holds',1),('after_call_holds',2),
                      ('retire',9),('same_identity',False),('selected_source',False),('attempts',1),('exception','java/lang/NullPointerException')):
            with self.subTest(key=key):self.rejected(mode,lambda r,key=key,v=v:lifetime(r).update(**{key:v}))
        mode='flint-portal-range-error-second'
        self.assertEqual(N.profile_for(mode)['range_error_at'],2)
        self.assertFalse(N.receipt_expected(N.profile_for(mode)))
        self.assertTrue(self.check(self.fresh_fixture(mode),mode)['matched'])
        def invented(rows):
            rows.insert(-2,('FLINT_RECEIPT ',dict(busy_before=9,before_holds=2,after_busy_holds=2,after_call_holds=1,retire=0,
                same_identity=True,selected_source=True,exception='java/lang/IllegalStateException',attempts=2)))
        self.rejected(mode,invented)
        mode=modes[0];rows=self.fresh_fixture(mode)
        rows[:]=[entry for entry in rows if entry[0]!='FLINT_RECEIPT ']
        outcome(rows).update(raw=11,returned=False,result=None,exception=None,native_policy=True,source_exception=False)
        actual=N.parse(text(rows,mode),mode) # Structurally valid native stop, with no invented source producer.
        with self.assertRaises(ValueError):N.validate(actual,mode,self.originals)


if __name__=='__main__':unittest.main()
