"""Asset-independent authoring and decoder controls; no native gameplay evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('lever_controls_subject',HERE/'test_lever_loop.py')
G=importlib.util.module_from_spec(spec)
spec.loader.exec_module(G)


def fixture():
    base={'index':0,'phase':'before','world':'a','time':0,'frames':0,'ticks':0,'world_ticks':0,
          'focus':1,'paused':0,'target':[0,0,0],'support':[1,0,0],'dust':[55,0,0],
          'light_count':0,'M':0,'skylight_subtraction':0,'redstone_particles':0,
          'random':{'seed':'000000000001','gaussian':'0'*16,'have':False},'E':[],'F':[]}
    states=[]
    for i,(phase,block,metadata,dust) in enumerate((('before',0,0,0),('input',69,4,0),
            ('input',69,12,15),('input',69,4,0),('input',69,12,15),
            ('powered',69,12,15),('saving',69,12,15),('reloaded',69,12,15))):
        row=copy.deepcopy(base)
        row.update(index=i,phase=phase,time=i,frames=i,ticks=i,world_ticks=i,
                   target=[block,metadata,0],dust=[55,dust,0])
        if phase=='reloaded':
            row['world']='b'
        states.append(row)
    suffix=''
    for i,(action,left,place,meta,dust) in enumerate((('place',False,True,4,0),
            ('on',False,False,12,15),('off',True,False,4,0),('powered',False,False,12,15))):
        suffix+=(f'LEVER_GUI_INPUT action={action} button={"left" if left else "right"} '
                 f'before-index={i} after-index={i+1} right-clicks={int(not left)} left-clicks={int(left)} '
                 f'block-uses={int(not left)} air-uses=0 admitted={int(place)} placed={int(place)} '
                 f'sounds={int(place)} count-stores={int(place)} used={int(place)} equip-reset={int(place)} '
                 f'cleared=0 swing=1 mining-starts={int(left)} mining-progress=0 mined=0 direct-suffix=1 '
                 f'target-metadata={meta} dust={dust} drained=1\n')
    suffix+='''LEVER_GUI_RETURN item=69 block=69 metadata=12 dust=15 count-before=2 count-after=1 damage=0 health=5 same-stack=1 used=1 hud=1 held=1 continuing=3 right-clicks=3 left-clicks=1
LEVER_GUI_SAVE sample=0 candidates=1 health=5 support=1 block=69 metadata=12 dust=15 time=6 source-save-title=1 pre-frame-owned-nbt=1 items=0
LEVER_GUI_RELOAD distinct-world=1 distinct-player=1 health=5 support=1 block=69 metadata=12 dust=15 item=69 count=1 damage=0 selected=0 hud=1 held=1 used=1 items=0
LEVER_GUI_FINISH cleanup=1 windows-absent=1 checks=10
'''
    for phase in ('before','placed','on','off','powered','reloaded'):
        suffix+=(f'LEVER_GUI_GEOMETRY phase={phase} vertices=100 bytes=3200 arrays=7 mode=4 '
                 'terrain-returned=1 type12-inclusion=source-control-flow skylight-subtraction=0 ao=1 section=0,64,0\n')
        suffix+=(f'LEVER_GUI_PICK phase={phase} id={1 if phase=="before" else 69} '
                 f'xyz=3,65,{3 if phase=="before" else 2} face=2 outline=1 vertices=18 draws=3 '
                 'shape=1 selected=1 expanded=1 shifted=1 camera-x=0000000000000000 '
                 'camera-y=0000000000000000 camera-z=0000000000000000\n')
        for batch,mode,vertices in ((0,3,5),(1,3,5),(2,1,8)):
            suffix+=(f'LEVER_GUI_OUTLINE phase={phase} batch={batch} mode={mode} vertices={vertices} '
                     f'words={vertices*8} submitted-bytes={vertices*32} captured=1\n')
    return states,suffix


def encoded(states,suffix):
    return ''.join('LEVER_GUI_STATE\t'+json.dumps(s)+'\n' for s in states)+suffix


class LeverControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m=G.modules()

    def test_plan_and_exact_authored_scene(self):
        plan=G.read_plan()
        self.assertEqual(plan['profile'],'wall-circuit')
        with tempfile.TemporaryDirectory(prefix='mc-lever-scene-') as temporary:
            path=Path(temporary)
            result=G.authored_regions(self.m,path)
            self.assertEqual(result['chunks'],441)
            self.assertEqual(result['other_compressed_records_preserved'],440)
            records,_=self.m['torch'].region_records(self.m['parser'],path)
            raw=b''.join(records[(0,0)][-1][key][1]
                         for key in ('Blocks','Data','SkyLight','BlockLight','HeightMap'))
            packet=G.ROOT/'tests/reference/client/lever_scene_geometry/scene-input.bin'
            self.assertEqual(raw,packet.read_bytes())

    def test_positive_decoder_only(self):
        states,suffix=fixture()
        result=G.observations(self.m,encoded(states,suffix))
        self.assertEqual(len(result['inputs']),4)
        self.assertEqual(len(result['outlines']),18)

    def test_state_identity_scalar_and_causal_corruptions(self):
        for change in (lambda s:s[0].update(index=True),lambda s:s[1].update(world='b'),
                       lambda s:s[2].update(target=[69,4,0]),lambda s:s[2].update(dust=[55,0,0]),
                       lambda s:s[3].update(paused=1),lambda s:s[4].update(world_ticks=3),
                       lambda s:s[7].update(world='a'),lambda s:s[1]['random'].update(have=1),
                       lambda s:s[1].update(skylight_subtraction=True),
                       lambda s:s[1].update(skylight_subtraction=16),lambda s:s[1].update(plate=[70,1,0]),
                       lambda s:s[1].update(redstone_particles=True),
                       lambda s:s[1].update(redstone_particles=-1),
                       lambda s:s[1].update(M=True)):
            states,suffix=fixture(); change(states)
            with self.subTest(change=change),self.assertRaises(ValueError):
                G.observations(self.m,encoded(states,suffix))

    def test_closed_receipts_and_input_suffix_corruptions(self):
        states,suffix=fixture(); raw=encoded(states,suffix)
        for before,after in (('before-index=0','before-index=2'),('after-index=1','after-index=0'),
                             ('button=left','button=right'),('swing=1','swing=0'),
                             ('mining-starts=1','mining-starts=0'),('drained=1','drained=0'),
                             ('used=1','used=2'),('count-after=1','count-after=2'),
                             ('ao=1','ao=0'),('face=2','face=1'),('words=40','words=39'),
                             ('section=0,64,0','section=0,0,0'),('checks=10','checks=0')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                G.observations(self.m,raw.replace(before,after,1))
        for bad in (raw+'LEVER_GUI_INVENTED x=1\n',
                    raw.replace('"index": 0','"index": 0, "index": 0',1),
                    raw.replace('"M": 0','"M": NaN',1),
                    raw.replace('LEVER_GUI_FINISH cleanup=1 windows-absent=1 checks=10\n','')):
            with self.subTest(bad=bad[-50:]),self.assertRaises(ValueError):
                G.observations(self.m,bad)


if __name__=='__main__':
    unittest.main()
