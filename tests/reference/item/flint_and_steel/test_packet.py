"""Changed operands, effects, ownership state and capture bindings must fail."""
import copy
import unittest
import packet as W


class FlintPacket(unittest.TestCase):
    def test_bound_original_packet(self):
        self.assertEqual(len(W.check()['rows']),104)

    def test_declared_model(self):
        self.assertEqual(len(W.profiles()),104)
        self.assertEqual(len(set(W.MODES)),104)
        for p in W.profiles():
            m=W.model(p)
            self.assertEqual(m['math_draws'],0)

    def test_finite_refusals(self):
        original=W.P.load(W.HERE/'finite.json')
        changes=(lambda v:v.update(schema='FLINT_AND_STEEL_V0'),
                 lambda v:v['rows'][0]['prepared'].update(item_seed=3),
                 lambda v:v['rows'][0]['prepared_class']['entity'].update(next_id=1),
                 lambda v:v['rows'][0]['before']['stack'].update(animation=0),
                 lambda v:v['rows'][0]['after']['stack'].update(damage=0),
                 lambda v:v['rows'][0]['events'][0].update(pitch='00000000'),
                 lambda v:v['rows'][0]['events'][0]['prepared_item_random'].__setitem__(0,'0'*16),
                 lambda v:v['rows'][0]['after']['chunk_planes'][0]['planes'].update(blocks='0'*64),
                 lambda v:v['rows'][0]['events'].reverse(),
                 lambda v:v['rows'][0]['events'][0]['stack_trace'].pop())
        for change in changes:
            value=copy.deepcopy(original);change(value)
            with self.assertRaises((ValueError,KeyError,TypeError)):W.validate_finite(value)

    def test_provenance_refusals(self):
        original=W.P.load(W.HERE/'provenance.json')
        for key,value in (('inputs_unchanged',False),('class_count',63),('rows_per_vm',103),('archive_sha256','0'*64)):
            changed=copy.deepcopy(original);changed[key]=value
            with self.assertRaises((ValueError,KeyError,TypeError)):W.validate_provenance(changed)

    def test_code_and_binding_refusals(self):
        original=W.P.load(W.HERE/'provenance.json')
        changes=(lambda v:v['methods']['ye.a(Liz;Lgs;Lfd;IIII)Z'].update(code_sha256='0'*64),
                 lambda v:v['sources'].update(**{'profiles.py':'0'*64}),
                 lambda v:v['shared'].update(**{'fire_model.py':'0'*64}),
                 lambda v:v['helpers'].update(**{'FlintAndSteelObservation.class':'0'*64}),
                 lambda v:v['original'][1].update(stdout_sha256='0'*64))
        for change in changes:
            value=copy.deepcopy(original);change(value)
            with self.assertRaises((ValueError,KeyError,TypeError)):W.validate_provenance(value)

    def test_source_order(self):
        cases={p['mode']:p for p in W.profiles()}
        direct=W.model(cases['flint-direct-stack-null'])
        self.assertEqual(direct['exception_origin']['pc'],130)
        self.assertTrue(direct['progress']['setter_returned'])
        wrapper=W.model(cases['flint-wrapper-stack-null'])
        self.assertTrue(wrapper['invocation_null'])
        self.assertIsNone(wrapper['exception_origin'])
        self.assertFalse(wrapper['progress']['wrapper_entered'])
        self.assertEqual(wrapper['events'],[])
        occupied=W.model(cases['flint-occupied-skips-random-fire'])
        self.assertEqual(occupied['exception'],'NONE')
        self.assertEqual(occupied['item_draws'],0)
        self.assertEqual(occupied['after']['stack']['damage'],1)
        broken=W.model(cases['flint-wrapper-break64'])
        self.assertEqual(broken['after']['stack']['count'],0)
        self.assertEqual(broken['after']['stack']['damage'],0)
        self.assertEqual(broken['after']['stats']['total'],[[16908547,1],[16974083,1]])


if __name__=='__main__':unittest.main()
