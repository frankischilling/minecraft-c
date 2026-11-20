"""Reject altered Ore input, model, entropy, effects and provenance evidence."""
import copy
import unittest
import packet as W


class OrePacket(unittest.TestCase):
    def test_bound_original_packet(self):
        self.assertEqual(len(W.check()['rows']),67)

    def test_declared_model(self):
        rows=W.profiles()
        self.assertEqual(len(rows),67)
        self.assertEqual(len(set(W.MODES)),67)
        for p in rows:
            m=W.model(p)
            self.assertEqual(m['math_draws'],0)
            self.assertGreaterEqual(m['world_draws'],0)

    def test_finite_refusals(self):
        original=W.P.load(W.HERE/'finite.json')
        changes=(lambda v:v.update(schema='REDSTONE_ORE_V0'),
                 lambda v:v['rows'][0]['prepared'].update(random_seed=2),
                 lambda v:v['rows'][0]['prepared_class']['scheduled'].update(sequence='0000000000000001'),
                 lambda v:v['rows'][0]['before']['target'].__setitem__(1,4),
                 lambda v:v['rows'][0]['after']['world_random'].__setitem__(0,'0'*16),
                 lambda v:v['rows'][0]['events'][0]['position'].__setitem__(0,'0'*16),
                 lambda v:v['rows'][0]['events'].reverse(),
                 lambda v:v['rows'][0]['after']['chunk_planes'][0]['planes'].update(metadata='0'*64),
                 lambda v:v['rows'][-1].update(exception='NONE'),
                 lambda v:v['rows'][0]['events'][0]['stack'].pop(),
                 lambda v:v['rows'][0]['after']['lighting']['regions'].clear())
        for change in changes:
            value=copy.deepcopy(original);change(value)
            with self.assertRaises((ValueError,KeyError,TypeError)):W.validate_finite(value)

    def test_provenance_refusals(self):
        original=W.P.load(W.HERE/'provenance.json')
        for key,value in (('inputs_unchanged',False),('class_count',60),('rows_per_vm',63),('archive_sha256','0'*64)):
            changed=copy.deepcopy(original);changed[key]=value
            with self.assertRaises((ValueError,KeyError,TypeError)):W.validate_provenance(changed)

    def test_code_and_binding_refusals(self):
        original=W.P.load(W.HERE/'provenance.json')
        changes=(lambda v:v['methods']['bs.h(Lfd;III)V'].update(code_sha256='0'*64),
                 lambda v:v['sources'].update(**{'profiles.py':'0'*64}),
                 lambda v:v['shared'].update(**{'fire_model.py':'0'*64}),
                 lambda v:v['helpers'].update(**{'RedstoneOreObservation.class':'0'*64}),
                 lambda v:v['original'][1].update(stdout_sha256='0'*64),
                 lambda v:v['original'][0].update(stderr_bytes=0),
                 lambda v:v['binaries'].update(client='0'*64))
        for change in changes:
            value=copy.deepcopy(original);change(value)
            with self.assertRaises((ValueError,KeyError,TypeError)):W.validate_provenance(value)

    def test_source_prefixes(self):
        cases={p['mode']:p for p in W.profiles()}
        self.assertEqual(W.model(cases['ore-height-0'])['world_draws'],18)
        self.assertEqual(len(W.model(cases['ore-height-0'])['events']),8)
        for mode,draws,pc in (('ore-provider-null',3,6),('ore-access-null',3,9),
                              ('ore-world-random-null',0,25),('ore-left-null-world-73',0,1)):
            value=W.model(cases[mode]);self.assertEqual(value['world_draws'],draws)
            self.assertEqual(value['exception_origin']['pc'],pc)
        inactive=W.model(cases['ore-tick-null-world-73'])
        self.assertEqual(inactive['exception'],'NONE')
        self.assertEqual(inactive['events'],[])
        for mode,method in (('ore-tick-access-null-74','b'),('ore-tick-access-null-cell-74','j')):
            value=W.model(cases[mode]);self.assertEqual(value['exception_origin']['method'],method)
            self.assertEqual(value['exception_origin']['pc'],9)
            self.assertEqual(value['events'],[])
            self.assertEqual(value['after']['target'],[73,0])


if __name__=='__main__':unittest.main()
