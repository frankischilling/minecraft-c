"""Bounded independent rejection classes over the selected original evidence."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
import packet as K


class Packet(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.finite=K.load('finite.json')['components'];cls.constructors=K.load('constructor-evidence.json')
        cls.trace=K.load('observed-trace.json');cls.pins=K.load('source-code.json')

    def test_qualified_packet(self):self.assertEqual(len(K.qualify()),30)

    def test_packet_byte_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for name in K.DIGESTS:(root/name).write_bytes((K.HERE/name).read_bytes())
            (root/'finite.json').write_bytes((root/'finite.json').read_bytes()+b' ')
            with self.assertRaisesRegex(ValueError,'packet fingerprint finite.json'):K.qualify(root)

    def test_process_receipt_failure_classes(self):
        base=K.load('provenance.json')['ordinary_receipts'][0]
        for name,value in [('exit',1),('exit',False),('timed_out',True),('output_overflow',True),('pipe_capture_complete',False),('reader_errors',{'stdout':'short write'})]:
            with self.subTest(name=name,value=value):
                row=copy.deepcopy(base);row[name]=value
                with self.assertRaises(ValueError):K.receipt(row)

    def test_ordinary_math_progress(self):
        rows=copy.deepcopy(self.constructors['ordinary'][0]);rows[1]['math_before']='000000000000'
        with self.assertRaisesRegex(ValueError,'shared-Math progress'):K.A.ordinary(rows)

    def test_ordinary_private_progress(self):
        rows=copy.deepcopy(self.constructors['ordinary'][0]);rows[1]['own_after']='000000000000'
        with self.assertRaises(ValueError):K.A.ordinary(rows)

    def test_final_velocity_width(self):
        row=copy.deepcopy(self.constructors['debugger'][1]);row['state']['velocity'][0]='0000000000000000'
        with self.assertRaisesRegex(ValueError,'velocity widths'):K.A.formula(row['state'],row['kind'],row['math_values'],row['own_values'])

    def test_flame_jitter_is_local_only(self):
        row=copy.deepcopy(self.constructors['debugger'][6]);self.assertEqual(row['kind'],'flame');row['state']['position'][0]='4021000000000001'
        with self.assertRaisesRegex(ValueError,'never republishes'):K.A.formula(row['state'],row['kind'],row['math_values'],row['own_values'])

    def test_distinct_original_and_mutable_scale(self):
        row=copy.deepcopy(self.constructors['debugger'][1]);row['state']['base_scale']='00000000'
        with self.assertRaisesRegex(ValueError,'subtype and mutable'):K.A.formula(row['state'],row['kind'],row['math_values'],row['own_values'])

    def test_source_lifetime(self):
        row=copy.deepcopy(self.constructors['debugger'][1]);row['state']['lifetime']+=1
        with self.assertRaisesRegex(ValueError,'color/age/lifetime'):K.A.formula(row['state'],row['kind'],row['math_values'],row['own_values'])

    def jdi_reject(self,change):
        rows=copy.deepcopy(self.constructors['debugger']);change(rows)
        with self.assertRaises(ValueError):K.A.debugger(rows,self.constructors['debugger_returned'],self.pins)

    def test_observed_private_return(self):self.jdi_reject(lambda rows:rows[1]['own_values'].__setitem__(0,'00000000'))
    def test_observed_math_return(self):self.jdi_reject(lambda rows:rows[1]['math_values'].__setitem__(0,'0000000000000000'))
    def test_private_seed_after_each(self):self.jdi_reject(lambda rows:rows[1]['own_after_each'].__setitem__(0,'000000000000'))
    def test_math_seed_after_each(self):self.jdi_reject(lambda rows:rows[1]['math_after_each'].__setitem__(0,'000000000000'))
    def test_actual_default_delegation(self):self.jdi_reject(lambda rows:rows[1]['methods'].pop(0))
    def test_live_constructor_code(self):self.jdi_reject(lambda rows:rows[1]['methods'][0].__setitem__('sha256','0'*64))
    def test_same_vm_returned_state(self):self.jdi_reject(lambda rows:rows[1]['state'].__setitem__('age',1))

    def trace_reject(self,change):
        events=copy.deepcopy(self.trace['events']);change(events)
        with self.assertRaises(ValueError):K.P.trace(events,self.trace['live_code'],self.pins,self.finite)

    def test_trace_continuity(self):self.trace_reject(lambda rows:rows[1].__setitem__('seq',99))
    def test_actual_mover_caller(self):self.trace_reject(lambda rows:next(r for r in rows if r['event']=='entry' and r['at']=='sn.b(DDD)V@0').__setitem__('caller','xn.w_()V@89'))
    def test_actual_failure_pc(self):self.trace_reject(lambda rows:next(r for r in rows if r['event']=='exception' and r['profile']==11).__setitem__('at','xn.w_()V@59'))
    def test_scale_before_super_failure(self):self.trace_reject(lambda rows:next(r for r in rows if r['event']=='entry' and r['at']=='xw.a(Lnw;FFFFFF)V@0' and r['profile']==31)['particle'].__setitem__('scale','40600000'))
    def test_vertex_call_count(self):self.trace_reject(lambda rows:next(r for r in rows if r['event']=='entry' and r['at']=='nw.a(DDDDD)V@0').__setitem__('at','nw.a(DDDDD)V@1'))
    def test_platform_exception_not_game_failure(self):self.trace_reject(lambda rows:next(r for r in rows if r['event']=='exception' and r['profile']==18).__setitem__('catch',None))

    def test_live_source_method_code(self):
        code=copy.deepcopy(self.trace['live_code']);code[0]['bytes']+=1
        with self.assertRaisesRegex(ValueError,'live source Code'):K.P.trace(self.trace['events'],code,self.pins,self.finite)

    def test_all_active_word_roles(self):
        # One exact row comparison per word role exercises distinct payload
        # positions without repeating whole-packet parsing for every vertex.
        for role in range(8):
            with self.subTest(role=role):
                rows=copy.deepcopy(self.finite);row=next(r for r in rows if r['nw'] and r['nw']['words']==48)
                row['packed'][role]=f'{int(row["packed"][role],16)^1:08x}'
                with self.assertRaises(ValueError):K.P.compare(rows,self.finite)

    def test_inactive_backing_projection(self):
        rows=copy.deepcopy(self.finite);next(r for r in rows if r['nw'])['packed'][99]='00000001'
        with self.assertRaises(ValueError):K.P.compare(rows,self.finite)

    def test_boolean_is_not_integer(self):
        rows=copy.deepcopy(self.finite);rows[0]['after']['dead']=0
        with self.assertRaises(ValueError):K.P.compare(rows,self.finite)

    def test_semantic_projection_hash(self):
        self.assertEqual(K.load('provenance.json')['component_semantic_sha256_per_vm'],[K.semantic_hash(self.finite)]*3)
        self.assertEqual(hashlib.sha256((K.HERE/'profiles.tsv').read_bytes()).hexdigest(),K.DIGESTS['profiles.tsv'])


if __name__=='__main__':unittest.main()
