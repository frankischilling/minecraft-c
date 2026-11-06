"""Pure prelaunch author/decoder controls, not native gameplay evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('rt_gui_controls_subject',HERE/'test_redstone_torch_loop.py')
G=importlib.util.module_from_spec(spec);spec.loader.exec_module(G)


def fixture():
    rng={'seed':'000000000001','gaussian':'0'*16,'have':False}
    base={'index':0,'phase':'before','world':'a','time':10,'frames':10,'ticks':10,'world_ticks':10,
          'focus':1,'paused':0,'target':[0,0,0],'support':[1,0,0],'plate':[70,1,0],'dust':[55,0,0],
          'light_count':0,'M':0,'random':rng,'E':[],'F':[],'new_future_target76':False}
    rows=[copy.deepcopy(base) for _ in range(4)]
    tick=['c',3,65,2,76,12,0]
    rows[1].update(index=1,phase='input',time=11,frames=11,ticks=11,world_ticks=11,target=[76,4,7],
                   E=[tick],F=[copy.deepcopy(tick)],new_future_target76=True)
    rows[2].update(index=2,phase='off',time=12,frames=12,ticks=12,world_ticks=12,target=[75,4,0])
    rows[3].update(index=3,phase='reloaded',world='b',time=12,frames=14,ticks=14,world_ticks=14,target=[75,4,0])
    suffix='''RT_GUI_INPUT kind=redstone-torch taps=1 right-clicks=1 block-uses=1 air-uses=0 admitted=1 placed=1 sounds=1 count-stores=1 used=1 cleared=0 equip-reset=1 drained=1 placed-hooks=source-control-flow onAdded-before-face=source-control-flow direct-suffix=1 metadata=0 target=3,65,2 face=2
RT_GUI_RETURN item=76 block=75 metadata=4 count-before=2 count-after=1 damage=0 health=5 same-stack=1 used=1 hud=1 held=1 continuing=3 future-due=12
RT_GUI_SAVE sample=0 candidates=1 health=5 support=1 block=75 metadata=4 plate=1 dust=0 time=12 source-save-title=1 pre-frame-owned-nbt=1 items=0
RT_GUI_RELOAD distinct-world=1 distinct-player=1 health=5 support=1 block=75 metadata=4 plate=1 dust=0 item=76 count=1 damage=0 selected=0 hud=1 held=1 used=1 items=0
RT_GUI_FINISH cleanup=1 windows-absent=1 checks=10
'''
    for phase in ('before','off','reloaded'):
        suffix+=f'RT_GUI_GEOMETRY phase={phase} vertices=90 bytes=2880 arrays=7 mode=4 terrain-returned=1 type2-inclusion=source-control-flow\n'
    return rows,suffix


def encoded(rows,suffix):
    return ''.join('RT_GUI_STATE\t'+json.dumps(r)+'\n' for r in rows)+suffix


class PureControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.m=G.modules()

    def test_frozen_plan_and_authored441_scene(self):
        plan=G.read_current_plan();self.assertEqual(plan['player']['Inventory'],[[0,76,2,0]])
        with tempfile.TemporaryDirectory(prefix='mc-redstone-torch-scene-') as temporary:
            path=Path(temporary)
            result=G.authored_regions(self.m,path)
            self.assertEqual(result['chunks'],441);self.assertEqual(result['other_compressed_records_preserved'],440)
        wire=self.m['torch'].metadata(self.m['world'],'Redstone Torch world',1304000000000,76)
        player=self.m['parser'].Nbt(wire).root()['Data'][1]['Player'][1]
        self.m['torch'].inventory(player,76,2)
        position=[struct.unpack('>Q',struct.pack('>d',x))[0] for x in plan['player']['Pos']]
        rotation=[struct.unpack('>I',struct.pack('>f',x))[0] for x in plan['player']['Rotation']]
        self.assertEqual(player['Pos'][1],[6,position])
        self.assertEqual(player['Rotation'][1],[5,rotation])

    def test_current_input_binding_changed_copies(self):
        controls=G.module('redstone_torch_current_input_controls',HERE/'scene_input_controls.py')
        controls.check_current_inputs(self,G,'redstone_torch')

    def test_historical_input_guard_remains_strict(self):
        binding=json.loads((G.ROOT/'tests/client/gameplay/current_scene_inputs.json').read_text())
        predecessor=binding['profiles']['redstone_torch']['predecessor']['source_input_sha256']
        changed=[name for name,pin in predecessor.items() if G.digest(G.ROOT/name)!=pin]
        self.assertEqual(changed,['tests/client/startup/test_world_selection_loop.c'])
        with self.assertRaises(ValueError):G.read_plan()

    def test_positive_synthetic_decoder_only(self):
        rows,suffix=fixture();value=G.observations(self.m,encoded(rows,suffix))
        self.assertEqual((value['first_lit'],value['first_off']),(1,2))

    def test_no_fake_future_deadline_or_identity_join(self):
        for change in (lambda r:r[1]['E'][0].__setitem__(5,14),
                       lambda r:r[1]['F'][0].__setitem__(0,'d'),
                       lambda r:r[1].update(new_future_target76=False),
                       lambda r:r[2].update(time=11),lambda r:r[3].update(world='a')):
            rows,suffix=fixture();change(rows)
            with self.subTest(change=change),self.assertRaises(ValueError):G.observations(self.m,encoded(rows,suffix))

    def test_source_input_result_and_scalar_corruptions(self):
        for change in (lambda r:r[1].update(target=[76,5,7]),lambda r:r[1].update(plate=[70,0,0]),
                       lambda r:r[2].update(target=[76,4,0]),lambda r:r[2].update(dust=[55,15,0]),
                       lambda r:r[1].update(focus=0),lambda r:r[1].update(paused=1),
                       lambda r:r[1].update(index=True),lambda r:r[1]['random'].update(have=1),
                       lambda r:r[1].update(M=True),lambda r:r[2].update(world_ticks=10),
                       lambda r:r[1].update(invented=1),lambda r:r[1]['E'][0].__setitem__(5,True)):
            rows,suffix=fixture();change(rows)
            with self.subTest(change=change),self.assertRaises(ValueError):G.observations(self.m,encoded(rows,suffix))

    def test_corrupted_closed_receipts_and_prefix(self):
        rows,suffix=fixture();raw=encoded(rows,suffix)
        for bad in (raw.replace('right-clicks=1','right-clicks=2'),raw.replace('face=2','face=1'),
                    raw.replace('used=1','used=0',1),raw.replace('count-after=1','count-after=2'),
                    raw.replace('checks=10','checks=0'),raw+'RT_GUI_INVENTED x=1\n',
                    raw.replace('"index": 0','"index": 0, "index": 0',1),raw.replace('"M": 0','"M": NaN',1),
                    raw.replace('RT_GUI_FINISH cleanup=1 windows-absent=1 checks=10\n','')):
            with self.subTest(bad=bad[-50:]),self.assertRaises(ValueError):G.observations(self.m,bad)


if __name__=='__main__':unittest.main()
