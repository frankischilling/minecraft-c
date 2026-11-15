"""Reject changed Fire operands, partial effects and original capture evidence."""
import copy
import unittest

import packet as W


class FirePacket(unittest.TestCase):
    def test_bound_original_packet(self):
        value=W.check()
        self.assertEqual(len(value['rows']),71)
        self.assertEqual(value,W.read_reference())
        self.assertEqual(len(W.class_pins()['classes']),58)

    def test_finite_refusals(self):
        original=W.P.load(W.HERE/'finite.json')
        changes=(
            lambda p:p.update(extra=0),lambda p:p.update(schema='FIRE_V0'),
            lambda p:p.update(scope='whole-game parity'),lambda p:p['rows'].reverse(),
            lambda p:p['rows'].pop(),lambda p:p['rows'][0].update(extra=0),
            lambda p:p['rows'][0].update(result=1),lambda p:p['rows'][0]['prepared'].update(extra=0),
            lambda p:p['rows'][0]['prepared'].update(entry_id=True),
            lambda p:p['rows'][9]['prepared']['cells'].reverse(),
            lambda p:p['rows'][23]['prepared'].update(random_seed=0),
            lambda p:p['rows'][31]['prepared'].update(biome='i'),
            lambda p:p['rows'][68]['prepared'].update(g=17),
            lambda p:p['rows'][68]['prepared'].update(xyz=[-41,64,-40]),
            lambda p:p['rows'][0]['receiver'].update(fire_class='uu'),
            lambda p:p['rows'][61]['receiver'].update(**{'class':'yq'}),
            lambda p:p['rows'][0]['prepared_class']['scheduled'].update(initialized=True),
            lambda p:p['rows'][59]['prepared_class']['scheduled'].update(sequence=p['rows'][59]['before']['scheduled']['sequence']),
            lambda p:p['rows'][45]['prepared_class']['entity'].update(next_id=1),
            lambda p:p['rows'][68]['prepared_class']['entity'].update(initialized=False),
            lambda p:p['rows'][0]['before'].update(target=[51,0]),
            lambda p:p['rows'][64]['before']['fire_arrays'].update(encouragement=[0]*256),
            lambda p:p['rows'][0]['after']['fire_arrays']['encouragement'].__setitem__(58,5),
            lambda p:p['rows'][0]['after']['fire_arrays']['flammability'].__setitem__(17,20),
            lambda p:p['rows'][23]['after'].update(target=[51,15]),
            lambda p:p['rows'][13]['after'].update(world_o=False),
            lambda p:p['rows'][9]['after'].update(target=[51,0]),
            lambda p:p['rows'][31]['after'].update(sky_seen=True),
            lambda p:p['rows'][0]['after']['scheduled'].update(sequence='0'*16),
            lambda p:p['rows'][65]['after']['scheduled'].update(ordered=[]),
            lambda p:p['rows'][68]['after']['scheduled']['ordered'][0].update(time='0000000000000028'),
            lambda p:p['rows'][34]['after']['scheduled']['members'].reverse(),
            lambda p:p['rows'][56]['after']['lighting']['regions'].reverse(),
            lambda p:p['rows'][57]['after']['lighting'].update(regions=[{'kind':'Sky','bounds':[16,64,15,16,65,15]}]),
            lambda p:p['rows'][0]['after']['lighting'].update(depth=True),
            lambda p:p['rows'][0]['after']['chunk_planes'][0]['planes'].update(blocks='0'*64),
            lambda p:p['rows'][41]['after']['chunk_planes'][0]['planes'].update(metadata='0'*64),
            lambda p:p['rows'][57]['after']['chunk_planes'][2]['planes'].update(height=p['rows'][56]['after']['chunk_planes'][2]['planes']['height']),
            lambda p:p['rows'][56]['after']['chunk_planes'][2]['planes'].update(sky='0'*64),
            lambda p:p['rows'][68]['after']['chunk_planes'].pop(),
            lambda p:p['rows'][68]['after']['chunk_planes'].reverse(),
            lambda p:p['rows'][68]['after']['random_work']['candidates'].reverse(),
            lambda p:p['rows'][68]['after']['random_work'].update(g=-1845269528),
            lambda p:p['rows'][68]['after']['random_work'].update(cooldown=6001),
            lambda p:p['rows'][59]['after']['weather'].update(rain_time=20),
            lambda p:p['rows'][68]['after'].update(time='0000000000000000'),
            lambda p:p['rows'][45]['after']['entities'][0].update(fuse=79),
            lambda p:p['rows'][45]['after']['entities'][0].update(id=1),
            lambda p:p['rows'][45]['after']['entities'][0]['motion'].__setitem__(0,'0'*16),
            lambda p:p['rows'][45]['after']['entities'][0]['bounds'].__setitem__(1,'0'*16),
            lambda p:p['rows'][53]['after'].update(entity_count=0),
            lambda p:p['rows'][68]['after']['players'][0].update(health=19),
            lambda p:p['rows'][68]['after']['players'][0].update(sleeping=True),
            lambda p:p['rows'][68]['after']['entity_class'].update(next_id=4),
            lambda p:p['rows'][45]['after']['world_random'].__setitem__(1,True),
            lambda p:p['rows'][45]['after']['math_random'].__setitem__(0,'0'*16),
            lambda p:p['rows'][13]['events'].pop(),lambda p:p['rows'][9]['events'].reverse(),
            lambda p:p['rows'][13]['events'][0].update(world_o=False),
            lambda p:p['rows'][56]['events'][0].update(xyz=[16,64,15,16,65,15]),
            lambda p:p['rows'][34]['events'][0].update(cell=[0,0]),
            lambda p:p['rows'][45]['events'][1]['entity'].update(fuse=40),
            lambda p:p['rows'][45]['events'][2].update(name='random.fizz'),
            lambda p:p['rows'][45]['events'][2].update(volume='3f000000'),
            lambda p:p['rows'][45]['events'][2].update(pitch='40000000'),
            lambda p:p['rows'][45]['events'][2]['position'].__setitem__(1,'4050200000000000'),
            lambda p:p['rows'][53]['events'][1]['math_random'].__setitem__(0,'0'*16),
            lambda p:p['rows'][57]['events'][0]['world_random'].__setitem__(0,'0'*16),
            lambda p:p['rows'][9]['events'][0]['stack'][2].update(method='other'),
            lambda p:p['rows'][9]['events'][0]['stack'][4].update(method='other'),
            lambda p:p['rows'][65].update(exception='NONE'),
            lambda p:p['rows'][65].update(exception_message='other'),
            lambda p:p['rows'][65]['exception_stack'].reverse(),
            lambda p:p['rows'][67]['exception_stack'][0].update(method='h'),
            lambda p:p['rows'][53]['exception_stack'][0].update(method='b'),
            lambda p:p['rows'][69]['prepared'].update(operation='tick'),
            lambda p:p['rows'][69]['prepared'].update(added_error_at=0),
            lambda p:p['rows'][70]['prepared'].update(sound_error_at=0),
            lambda p:p['rows'][69].update(exception='NONE'),
            lambda p:p['rows'][70].update(exception_message='other'),
            lambda p:p['rows'][69]['after'].update(entity_count=0),
            lambda p:p['rows'][69]['after']['entities'].pop(),
            lambda p:p['rows'][70]['after']['entities'][0].update(fuse=79),
            lambda p:p['rows'][69]['after']['scheduled']['ordered'].pop(),
            lambda p:p['rows'][70]['after']['scheduled']['members'].pop(),
            lambda p:p['rows'][69]['after']['scheduled']['ordered'][0].update(time='0000000000000028'),
            lambda p:p['rows'][69]['after']['random_work'].update(cooldown=6000),
            lambda p:p['rows'][70]['after'].update(time='0000000000000000'),
            lambda p:p['rows'][69]['events'][1]['entities'].pop(),
            lambda p:p['rows'][70]['events'][2].update(pitch='3f000000'),
            lambda p:p['rows'][69]['exception_stack'].__delitem__(-3),
            lambda p:p['rows'][70]['exception_stack'].__delitem__(-4),
        )
        for index,change in enumerate(changes):
            damaged=copy.deepcopy(original);change(damaged)
            with self.subTest(index=index),self.assertRaises(ValueError):W.validate_finite(damaged)

    def test_provenance_refusals(self):
        original=W.P.load(W.HERE/'provenance.json')
        changes=(
            lambda p:p.update(extra=0),lambda p:p.update(schema='FIRE_PROVENANCE_V0'),
            lambda p:p.update(status='PASS'),lambda p:p.update(archive_sha256='0'*64),
            lambda p:p.update(finite_sha256='0'*64),lambda p:p.update(capture_sha256='x'),
            lambda p:p.update(class_count=True),lambda p:p.update(class_count=57),
            lambda p:p.update(rows_per_vm=68),lambda p:p.update(inputs_unchanged=False),
            lambda p:p['sources'].pop('model.py'),lambda p:p['sources'].pop('hashset_order.py'),
            lambda p:p['sources'].update(**{'packet.py':'0'*64}),lambda p:p['sources'].update(extra='0'*64),
            lambda p:p['shared'].pop('setup'),lambda p:p['helpers'].pop('FireObservation.class'),
            lambda p:p['methods'].pop('yq.a(Lfd;IIILjava/util/Random;)V'),
            lambda p:p['binaries'].update(client='0'*64),lambda p:p['original'].pop(),
            lambda p:p['original'][0].update(exit=1),lambda p:p['original'][0].update(timed_out=True),
            lambda p:p['original'][0].update(pipe_capture_complete=False),
            lambda p:p['original'][0].update(output_overflow=True),
            lambda p:p['original'][0].update(stdout_bytes=0),lambda p:p['original'][0].update(timeout_seconds=71),
            lambda p:p['original'][1].update(stdout_sha256='0'*64),
            lambda p:p['processes']['compile'].update(exit=True),lambda p:p['processes'].pop('bytecode'),
        )
        for index,change in enumerate(changes):
            damaged=copy.deepcopy(original);change(damaged)
            with self.subTest(index=index),self.assertRaises(ValueError):W.validate_provenance(damaged)

    def test_method_and_class_pin_refusals(self):
        original=W.P.load(W.HERE/'fire-method-pins.json')
        changes=(
            lambda p:p.update(extra=0),lambda p:p.update(archive_sha256='0'*64),
            lambda p:p['classes'].pop('jp'),lambda p:p['classes'].update(extra={}),
            lambda p:p['classes']['yq'].update(class_bytes=True),
            lambda p:p['classes']['qw'].update(super='ls'),
            lambda p:p['classes']['ak']['methods'][0].update(code_length=True),
            lambda p:p['methods'].pop('yq.a(Lfd;IIILjava/util/Random;)V'),
            lambda p:p['methods']['yq.c(Lfd;III)V'].update(code_length=True),
        )
        for index,change in enumerate(changes):
            damaged=copy.deepcopy(original);change(damaged)
            with self.subTest(index=index),self.assertRaises(ValueError):W.validate_class_pins(damaged)

    def test_json_transport(self):
        for text in ('{"a":0,"a":1}','{"v":NaN}','{"v":Infinity}','{"v":-Infinity}'):
            with self.subTest(text=text),self.assertRaises(ValueError):W.P.loads(text)

    def test_independent_effect_model(self):
        cases={p['mode']:W.model(p) for p in W.profiles()}
        self.assertEqual(cases['fire-tick-age15-extinguish']['after']['target'],[0,0])
        self.assertEqual(cases['fire-tick-age15-extinguish']['world_draws'],1)
        self.assertEqual(cases['fire-tick-spread-seed8']['after']['scheduled']['sequence'],'0000000000000002')
        self.assertEqual(cases['fire-added-portal-cell-error-third']['after']['world_o'],True)
        self.assertEqual(len(cases['fire-added-portal-x']['events']),7)
        self.assertEqual(cases['fire-tick-tnt-added-error']['math_draws'],2)
        self.assertEqual(cases['fire-tick-tnt-cell-error-before-prime']['math_draws'],0)
        self.assertEqual(cases['fire-tick-b-null']['exception_origin']['pc'],12)
        self.assertEqual(cases['fire-tick-a-null']['exception_origin']['pc'],14)
        self.assertEqual(cases['fire-neighbor-a-null-provider-null']['exception_origin']['pc'],6)
        random=cases['fire-world-random-player']
        self.assertEqual(random['block_ticks'],1)
        self.assertEqual(random['after']['random_work']['g'],-953057917)
        self.assertEqual(len(random['after']['random_work']['candidates']),361)
        self.assertEqual(random['after']['random_work']['candidates'][0],[-4,-4])
        self.assertEqual(random['after']['world_random'][0],'000094fcd55ab53f')
        r=W.RandomModel(1)
        self.assertEqual((r.bounded(100000),r.bounded(16)),(48985,1))
        for profile in W.profiles():
            arrays=W.plane_arrays(profile,True)
            self.assertEqual(len(arrays),361 if profile['random_player'] else 4)
            self.assertTrue(all({k:len(v) for k,v in row['planes'].items()}==dict(blocks=32768,metadata=16384,sky=16384,block_light=16384,height=256) for row in arrays))

    def test_scheduled_tnt_failure_prefixes(self):
        cases={p['mode']:W.model(p) for p in W.profiles()}
        for kind in ('added','sound'):
            result=cases['fire-world-tick-tnt-'+kind+'-error']
            self.assertEqual(result['exception'],'java/lang/IllegalStateException')
            self.assertEqual(result['exception_message'],'Fire '+kind+' observer interruption')
            self.assertEqual(result['exception_source_stack'][-2:],['fd.a','fd.l'])
            self.assertEqual((result['world_draws'],result['math_draws']),(4,2))
            self.assertEqual(result['after']['time'],'0000000000000001')
            self.assertEqual(result['after']['weather']['rain_time'],19)
            self.assertEqual(result['after']['weather']['thunder_time'],29)
            self.assertEqual(result['after']['random_work'],dict(g=17,cooldown=6001,candidates=[]))
            self.assertEqual(result['after']['entity_count'],1)
            self.assertEqual(result['after']['entities'][0]['fuse'],80)
            self.assertEqual(len(result['after']['scheduled']['ordered']),2)
            self.assertEqual(len(result['after']['scheduled']['members']),2)
            self.assertEqual([r['time'] for r in result['after']['scheduled']['ordered']],['0000000000000029']*2)
            self.assertEqual([e['kind'] for e in result['events']],['cell','added']+(['sound'] if kind=='sound' else []))


if __name__=='__main__':unittest.main()
