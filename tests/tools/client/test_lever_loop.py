"""Windows saved Lever circuit through ordinary client input and save/reselection."""
import argparse
import copy
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SCENE = ROOT / 'tests/client/gameplay/lever_scene.json'
PLAN_SHA = 'eb7d6bac7804dea8a6b28240acacfe9fabc07d60978981b31394628f615753c6'
STATE_KEYS = set(('index phase world time frames ticks world_ticks focus paused target support dust '
                  'light_count M skylight_subtraction redstone_particles random E F').split())
CELLS = ((3,64,0,1,0), (3,64,3,3,0), (3,65,3,1,0), (3,66,3,0,0),
         (3,65,2,0,0), (4,64,2,1,0), (4,65,2,55,0))


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def modules():
    return {name: module('lever_gui_' + name, ROOT / path) for name,path in {
        'world':'tests/tools/client/test_world_selection_loop.py',
        'mining':'tests/tools/client/test_mining_loop.py',
        'farming':'tests/tools/client/test_farming_loop.py',
        'parser':'tests/tools/world/test_source_saving.py',
        'torch':'tests/tools/client/test_torch_placement_loop.py'}.items()}


def read_plan():
    require(PLAN_SHA is not None and digest(SCENE) == PLAN_SHA, 'frozen prelaunch Lever plan')
    value = json.loads(SCENE.read_text())
    require(all(digest(ROOT/n) == pin for n,pin in value['source_input_sha256'].items()),
            'unchanged inherited fixture inputs')
    require(value['profile'] == 'wall-circuit', 'declared Lever circuit profile')
    return value


def nibble(values, at):
    return (values[at//2] >> ((at&1)*4)) & 15


def nibble_set(values, at, value):
    shift = (at&1)*4
    values[at//2] = (values[at//2] & ~(15 << shift)) | (value << shift)


def authored_regions(m, directory):
    world,parser,farming,torch = [m[k] for k in ('world','parser','farming','torch')]
    base = torch.authored_regions(world,parser,farming,directory)
    old,images = torch.region_records(parser,directory)
    name,location,stamp,allocation,payload,wire,level = old[(0,0)]
    arrays = {key:bytearray(level[key][1]) for key in ('Blocks','Data','SkyLight','HeightMap')}
    at = lambda x,y,z:(x<<11)|(z<<7)|y
    for x,y,z,block,meta in CELLS:
        index=at(x,y,z)
        arrays['Blocks'][index]=block
        nibble_set(arrays['Data'],index,meta)
    for x,y,z,light in ((3,64,3,0),(3,65,3,0),(3,66,3,15),(3,67,3,15),
                         (4,64,2,0),(4,65,2,15)):
        nibble_set(arrays['SkyLight'],at(x,y,z),light)
    arrays['HeightMap'][3*16+3]=66
    arrays['HeightMap'][2*16+4]=65
    for key,new in arrays.items():
        old_tag=world.tag(7,key,struct.pack('>i',len(level[key][1]))+level[key][1])
        require(wire.count(old_tag)==1,'one declared Chunk array field')
        wire=wire.replace(old_tag,world.tag(7,key,struct.pack('>i',len(new))+new))
    compressed=zlib.compress(wire)
    record=struct.pack('>I',len(compressed)+1)+b'\2'+compressed
    sector,count=location>>8,location&255
    require(len(record)<=count*4096,'same finite selected Chunk sector allocation')
    image=images[name]
    image[sector*4096:(sector+count)*4096]=record+bytes(count*4096-len(record))
    (directory/name).write_bytes(image)
    actual,_=torch.region_records(parser,directory)
    require(set(actual)==set(old) and len(actual)==441,'exact441 saved Chunk coordinates')
    require(all(actual[c][:-1]==old[c][:-1] for c in old if c!=(0,0)),
            'all440 other compressed records byte-exact')
    current=actual[(0,0)][-1]
    require(actual[(0,0)][1:3]==(location,stamp),'selected Chunk slot and timestamp unchanged')
    for key,new in arrays.items():
        require(current[key] == [7,bytes(new)],'complete independently decoded authored array')
    for key in set(level)-set(arrays):
        require(current[key] == level[key], 'unchanged selected Chunk field '+key)
    for x,y,z,block,meta in CELLS:
        index=at(x,y,z)
        require(current['Blocks'][1][index]==block and nibble(current['Data'][1],index)==meta,
                'independent declared saved cell decode')
    require(current['BlockLight'][1]==bytes(16384),'all authored BlockLight remains zero')
    return {'base':base,'revision':'LEVER_GUI_WALL_DUST_NO_PLATE','chunks':441,
            'changed_chunks':[[0,0]],'other_compressed_records_preserved':440,
            'selected_slot_timestamp_allocation_preserved':True,
            'selected_wire_sha256':hashlib.sha256(wire).hexdigest(),
            'planes_sha256':{k:hashlib.sha256(current[k][1]).hexdigest()
                             for k in ('Blocks','Data','SkyLight','BlockLight','HeightMap')}}


def unique(pairs):
    value={}
    for key,item in pairs:
        require(key not in value,'duplicate JSON state key')
        value[key]=item
    return value


def loads(value):
    return json.loads(value,object_pairs_hook=unique,
                      parse_constant=lambda x:(_ for _ in ()).throw(ValueError('non-JSON scalar')))


def integer(value,bits=64):
    return type(value) is int and -(1<<(bits-1))<=value<(1<<(bits-1))


def state_rows(stdout):
    states=[]
    for line in stdout.splitlines():
        if not line.startswith('LEVER_GUI_STATE\t'):
            continue
        row=loads(line.split('\t',1)[1])
        require(type(row) is dict and set(row)==STATE_KEYS,'closed passive state schema')
        require(type(row['index']) is int and row['index']==len(states) and row['phase'] in
                ('bootstrap','loading','before','input','placed','on','off','powered','continuing',
                 'saving','reloading','reloaded') and type(row['world']) is str and
                re.fullmatch('[1-9a-f][0-9a-f]*',row['world']),'ordered actual state identity')
        require(all(integer(row[k]) for k in ('time','frames','ticks','world_ticks','light_count','M'))
                and all(row[k]>=0 for k in ('frames','ticks','world_ticks','light_count')) and
                all(type(row[k]) is int and row[k] in (0,1) for k in ('focus','paused')) and
                type(row['skylight_subtraction']) is int and 0<=row['skylight_subtraction']<=15,
                'typed actual state scalars')
        require(type(row['redstone_particles']) is int and row['redstone_particles']>=0,
                'passive live READY Redstone particle count')
        for key in ('target','support','dust'):
            cell=row[key]
            require(type(cell) is list and len(cell)==3 and all(type(x) is int for x in cell)
                    and 0<=cell[0]<=255 and all(0<=x<=15 for x in cell[1:]),'typed actual cell')
        rng=row['random']
        require(type(rng) is dict and set(rng)=={'seed','gaussian','have'} and
                type(rng['seed']) is str and re.fullmatch('[0-9a-f]{12}',rng['seed']) and
                type(rng['gaussian']) is str and re.fullmatch('[0-9a-f]{16}',rng['gaussian']) and
                type(rng['have']) is bool,'unmodified passive Random bits')
        for collection in ('E','F'):
            require(type(row[collection]) is list,'actual scheduler inventory')
            for tick in row[collection]:
                require(type(tick) is list and len(tick)==7 and type(tick[0]) is str and
                        re.fullmatch('[1-9a-f][0-9a-f]*',tick[0]) and
                        all(integer(x,32) for x in tick[1:5]) and all(integer(x) for x in tick[5:]),
                        'full actual scheduled record')
        require(sorted(row['E'])==sorted(row['F']),'same actual E/F identities and complete fields')
        if states:
            require(all(row[k]>=states[-1][k] for k in ('frames','ticks','world_ticks')),
                    'monotone completed native counters')
        states.append(row)
    require(states,'actual returned-frame state observations required')
    return states


def observations(m,stdout):
    farming=m['farming']; states=state_rows(stdout)
    tags={'STATE','INPUT','RETURN','SAVE','RELOAD','FINISH','GEOMETRY','PICK','OUTLINE'}
    require(all(line.split(None,1)[0].split('\t',1)[0] in {'LEVER_GUI_'+x for x in tags}
                for line in stdout.splitlines() if line.startswith('LEVER_GUI_')),'closed receipt tags')
    rows={}
    for tag in ('RETURN','SAVE','RELOAD','FINISH'):
        found=farming.rows(stdout,'LEVER_GUI_'+tag)
        require(len(found)==1,'one complete '+tag+' receipt')
        rows[tag.lower()]=found[0]
    inputs=farming.rows(stdout,'LEVER_GUI_INPUT')
    require(len(inputs)==4,'exactly four input taps')
    previous_after=-1
    for inp,action,button,before,after in zip(inputs,('place','on','off','powered'),
            ('right','right','left','right'),((0,0,0),(69,4,0),(69,12,15),(69,4,0)),
            ((69,4,0),(69,12,15),(69,4,0),(69,12,15))):
        left=button=='left'; place=action=='place'
        expected={'action':action,'button':button,'right-clicks':str(int(not left)),
                  'left-clicks':str(int(left)),'block-uses':str(int(not left)),
                  'air-uses':'0','admitted':str(int(place)),'placed':str(int(place)),
                  'sounds':str(int(place)),'count-stores':str(int(place)),'used':str(int(place)),
                  'equip-reset':str(int(place)),'cleared':'0','swing':'1','mining-starts':str(int(left)),
                  'mining-progress':'0','mined':'0','target-metadata':str(after[1]),
                  'dust':str(after[2]),'drained':'1'}
        require(set(inp)==set(expected)|{'direct-suffix','before-index','after-index'} and
                all(inp[k]==v for k,v in expected.items()) and inp['direct-suffix'] in ('0','1')
                and all(re.fullmatch('0|[1-9][0-9]*',inp[k]) for k in ('before-index','after-index')),
                'complete discrete input suffix for '+action)
        a,b=int(inp['before-index']),int(inp['after-index'])
        require(previous_after<=a<b<len(states),'ordered actual before/after input witnesses')
        for index,expected_state in ((a,before),(b,after)):
            state=states[index]
            require(state['target'][:2]==list(expected_state[:2]) and
                    state['dust'][:2]==[55,expected_state[2]] and state['support'][0]==1 and
                    state['focus']==1 and state['paused']==0,'actual input cell/focus witness')
        require(states[a]['world']==states[b]['world'] and states[b]['world_ticks']>states[a]['world_ticks']
                and states[b]['phase']=='input','actual later consumed input tick')
        previous_after=b
    ret=rows['return']; save=rows['save']; loaded=rows['reload']; finish=rows['finish']
    require(ret=={'item':'69','block':'69','metadata':'12','dust':'15','count-before':'2',
                 'count-after':'1','damage':'0','health':'5','same-stack':'1','used':'1','hud':'1',
                 'held':'1','continuing':'3','right-clicks':'3','left-clicks':'1'},'returned powered state')
    expected={'health':'5','support':'1','block':'69','metadata':'12','dust':'15',
              'source-save-title':'1','pre-frame-owned-nbt':'1','items':'0'}
    require(set(save)==set(expected)|{'sample','candidates','time'} and
            all(save[k]==v for k,v in expected.items()) and save['sample'].isdigit() and
            save['candidates'].isdigit() and 0<=int(save['sample'])<int(save['candidates']) and
            re.fullmatch('0|-?[1-9][0-9]*',save['time']),'selected actual paused save entry')
    require(loaded=={'distinct-world':'1','distinct-player':'1','health':'5','support':'1','block':'69',
                    'metadata':'12','dust':'15','item':'69','count':'1','damage':'0','selected':'0',
                    'hud':'1','held':'1','used':'1','items':'0'},'actual reloaded powered circuit')
    require(set(finish)=={'cleanup','windows-absent','checks'} and
            finish['cleanup']==finish['windows-absent']=='1' and finish['checks'].isdigit() and
            int(finish['checks'])>0,'terminal ownership and window cleanup')
    require(len({s['world'] for s in states})==2,'two actual World owners')
    geometry=farming.rows(stdout,'LEVER_GUI_GEOMETRY')
    phases=('before','placed','on','off','powered','reloaded')
    require(len(geometry)==6 and tuple(g['phase'] for g in geometry)==phases,'six actual mesh phases')
    for g in geometry:
        require(set(g)==set(('phase vertices bytes arrays mode terrain-returned type12-inclusion '
                            'skylight-subtraction ao section').split()) and g['vertices'].isdigit() and
                int(g['vertices'])>0 and g['bytes'].isdigit() and int(g['bytes'])==int(g['vertices'])*32 and
                g['arrays']=='7' and g['mode']=='4' and g['terrain-returned']=='1' and
                g['type12-inclusion']=='source-control-flow' and g['ao']=='1' and g['section']=='0,64,0' and
                g['skylight-subtraction'].isdigit() and 0<=int(g['skylight-subtraction'])<=15,
                'actual compiled section and default AO operands')
    picks=farming.rows(stdout,'LEVER_GUI_PICK'); outlines=farming.rows(stdout,'LEVER_GUI_OUTLINE')
    require(len(picks)==6 and tuple(p['phase'] for p in picks)==phases and len(outlines)==18,
            'complete actual picking/outline phases')
    for g in geometry:
        match=[p for p in picks if p['phase']==g['phase']][0]
        expected={'phase':g['phase'],'id':'1' if g['phase']=='before' else '69',
                  'xyz':'3,65,3' if g['phase']=='before' else '3,65,2','face':'2','outline':'1',
                  'vertices':'18','draws':'3','shape':'1','selected':'1','expanded':'1','shifted':'1'}
        require(set(match)==set(expected)|{'camera-x','camera-y','camera-z'} and
                all(match[k]==v for k,v in expected.items()) and
                all(re.fullmatch('[0-9a-f]{16}',match['camera-'+k]) for k in ('x','y','z')),
                'actual selected source box and camera')
        group=[p for p in outlines if p['phase']==g['phase']]
        for row,i,mode,vertices in zip(group,range(3),(3,3,1),(5,5,8)):
            expected={'phase':g['phase'],'batch':str(i),'mode':str(mode),'vertices':str(vertices),
                      'words':str(vertices*8),'submitted-bytes':str(vertices*32),'captured':'1'}
            require(row==expected,'complete copied source outline batch')
        require(len(group)==3,'exactly three outline batches per phase')
    return dict(rows,inputs=inputs,states=states,geometry=geometry,picks=picks,outlines=outlines)


def readback(m,evidence,receipt,authored):
    parser,mining,torch=[m[k] for k in ('parser','mining','torch')]
    owned=lambda n:parser.Nbt(parser.read(evidence/n)).root()
    before,on=owned('lever-player-before.nbt'),owned('lever-player-on.nbt')
    torch.inventory(before,69,2); torch.inventory(on,69,1)
    mining.same_fields(authored,before,('Pos','Rotation','Health','Score','Dimension','Inventory'),
                      'saved input versus actual loaded Player')
    saved=evidence/'snapshots/lever-saved/alpha'
    player=parser.inflate(parser.read(saved/'level.dat'),31)['Data'][1]['Player'][1]
    entry=owned(f"lever-player-entry-{int(receipt['save']['sample']):04}.nbt")
    reopened=owned('lever-player-reloaded.nbt')
    for value in (player,entry,reopened):
        torch.inventory(value,69,1)
    mining.same_fields(player,entry,('Pos','Motion','Rotation','Health','Score','Dimension','Inventory'),
                      'actual selected Player save body')
    mining.same_fields(player,reopened,('Health','Score','Dimension','Inventory'),'actual reloaded Player')
    info=parser.inflate(parser.read(saved/'level.dat'),31)['Data'][1]
    require(info['Time']==[4,int(receipt['save']['time'])],'independent saved WorldTime')
    level=m['farming'].selected_chunk(parser,saved/'region/r.0.0.mcr')[4]
    for x,y,z,block,meta in ((3,65,2,69,12),(3,65,3,1,0),(3,66,3,0,0),(4,65,2,55,15)):
        at=(x<<11)|(z<<7)|y
        require(level['Blocks'][1][at]==block and nibble(level['Data'][1],at)==meta,
                'independent saved powered Lever and Dust cells')
    target=(3<<11)|(2<<7)|65
    require(nibble(level['BlockLight'][1],target)==0,'saved nonemitting Lever BlockLight0')
    items,chunks=mining.saved_items(parser,saved)
    require(not items and chunks==441,'all441 saved Chunks and no Item entities')
    return {'target':[3,65,2,69,12],'dust':[4,65,2,55,15],'saved_time':int(receipt['save']['time']),
            'Inventory':[[0,69,1,0]],'saved_items':0,'saved_chunks':chunks}


def geometry_readback(evidence,receipt):
    geometry=module('lever_gui_geometry',HERE/'lever_geometry.py')
    proof=[]
    points=((0,0,0),(1,0,0),(1,0,1),(0,0,1),(0,0,0),
            (0,1,0),(1,1,0),(1,1,1),(0,1,1),(0,1,0),
            (0,0,0),(0,1,0),(1,0,0),(1,1,0),(1,0,1),(1,1,1),(0,0,1),(0,1,1))
    f32=lambda value:struct.unpack('<f',struct.pack('<f',value))[0]
    for row in receipt['geometry']:
        phase=row['phase']; present=phase!='before'; powered=phase in ('on','powered','reloaded')
        wire=(evidence/('lever-geometry-'+phase+'.bin')).read_bytes()
        require(len(wire)==int(row['bytes']),'actual copied terrain byte extent')
        value=geometry.check_geometry(wire,True,int(row['skylight-subtraction']),powered,present)
        value['cells']=geometry.check_cells((evidence/('lever-cells-'+phase+'.bin')).read_bytes(),powered,present)
        pick=next(p for p in receipt['picks'] if p['phase']==phase)
        camera=[struct.unpack('>d',bytes.fromhex(pick['camera-'+axis]))[0] for axis in ('x','y','z')]
        require(all(abs(x)<32000128 for x in camera),'finite actual scene camera')
        # XR metadata4 selected bounds, not its narrower render base bounds.
        selected=([3+.3125,65+f32(.2),2+.625,3+.6875,65+f32(.8),3.]
                  if present else [3.,65.,3.,4.,66.,4.])
        epsilon=f32(.002)
        expanded=[v+(-epsilon if i<3 else epsilon) for i,v in enumerate(selected)]
        shifted=[v-camera[i%3] for i,v in enumerate(expanded)]
        expected=[struct.pack('<3f',*(shifted[j+(3 if index[j] else 0)] for j in range(3))) for index in points]
        actual=[]
        for batch,vertices in enumerate((5,5,8)):
            raw=(evidence/f'lever-outline-{phase}-{batch}.bin').read_bytes()
            require(len(raw)==vertices*32,'exact copied outline extent')
            actual.extend(raw[i:i+12] for i in range(0,len(raw),32))
        require(actual==expected,'all18 selected-box/float-epsilon/camera-shifted outline positions')
        value['outline_positions']=18
        proof.append(dict(value,phase=phase))
    return proof


def corruption_controls(m,stdout,evidence,receipt):
    """Reject changed copies of this execution's evidence; retained bytes stay intact."""
    rejected=0

    def reject(call):
        nonlocal rejected
        try:
            call()
        except (ValueError,AssertionError):
            rejected+=1
            return
        raise ValueError('corrupted actual Lever evidence was accepted')

    for tag,before,after in (
            ('INPUT action=off','button=left','button=right'),
            ('INPUT action=off','mining-starts=1','mining-starts=0'),
            ('INPUT action=place','swing=1','swing=0'),
            ('INPUT action=on','target-metadata=12','target-metadata=13'),
            ('INPUT action=place','drained=1','drained=0'),
            ('RETURN','count-after=1','count-after=2'),
            ('RELOAD','distinct-world=1','distinct-world=0'),
            ('GEOMETRY phase=on','ao=1','ao=0'),
            ('OUTLINE phase=on batch=0','submitted-bytes=160','submitted-bytes=159')):
        lines=[line for line in stdout.splitlines() if line.startswith('LEVER_GUI_'+tag+' ')]
        require(len(lines)==1 and before in lines[0],'unique actual corruption operand exists')
        changed=stdout.replace(lines[0],lines[0].replace(before,after,1),1)
        reject(lambda changed=changed:observations(m,changed))
    geometry=module('lever_corruption_geometry',HERE/'lever_geometry.py')
    row=next(g for g in receipt['geometry'] if g['phase']=='on')
    raw=(evidence/'lever-geometry-on.bin').read_bytes()
    sky=int(row['skylight-subtraction'])
    selected=geometry.check_geometry(raw,True,sky,True)['lever']
    start=selected['start_vertex']*32
    end=start+selected['vertices']*32
    for word in range(6):
        changed=bytearray(raw); changed[start+word*4]^=1
        reject(lambda changed=bytes(changed):geometry.check_geometry(changed,True,sky,True))
    reject(lambda:geometry.check_geometry(raw+raw[start:end],True,sky,True))
    reject(lambda:geometry.check_geometry(raw[:start]+raw[start+32:],True,sky,True))
    cells=bytearray((evidence/'lever-cells-on.bin').read_bytes())
    cells[(3<<11)|(2<<7)|65]=0
    reject(lambda:geometry.check_cells(bytes(cells),True))
    changed=copy.deepcopy(receipt)
    pick=next(p for p in changed['picks'] if p['phase']=='on')
    camera=struct.unpack('>d',bytes.fromhex(pick['camera-x']))[0]
    pick['camera-x']=struct.pack('>d',camera+1).hex()
    reject(lambda:geometry_readback(evidence,changed))
    return {'rejected':rejected,'scope':'Changed copies of actual returned input/geometry/cell/outline evidence'}


def input_paths(executable):
    inputs={p for p in HERE.iterdir() if p.is_file()}
    inputs.update((executable,ROOT/'CMakeLists.txt',SCENE))
    inputs.update(executable.parent.glob('*.dll'))
    for folder in ('src','include','cmake','tests/client','tests/tools/client','tests/tools/world',
                   'tests/reference/client/lever_scene_geometry'):
        inputs.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and
                      p.suffix in ('.c','.h','.inc','.py','.cmake','.json','.java','.bin','.txt'))
    return inputs


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--assets',type=Path,required=True)
    parser.add_argument('--artifacts',type=Path,required=True)
    args=parser.parse_args()
    require(os.name=='nt','Windows GUI fixture; no implicit platform launch')
    plan=read_plan(); m=modules()
    world,torch,nbt,mining=[m[k] for k in ('world','torch','parser','mining')]
    executable,assets=args.executable.resolve(strict=True),args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True,exist_ok=True)
    evidence=Path(tempfile.mkdtemp(prefix='lever-wall-circuit-',dir=args.artifacts.resolve()))
    for name in ('snapshots','entries'):
        (evidence/name).mkdir()
    inputs=input_paths(executable)
    before_inputs={str(p):digest(p) for p in sorted(inputs)}
    before_assets=world.snapshot(assets)
    report={'status':'FAIL','scope':__doc__,'plan':plan,'inputs_before':before_inputs,
            'assets_before':before_assets}
    (evidence/'run.json').write_text(json.dumps(report,indent=2)+'\n')
    try:
        with tempfile.TemporaryDirectory(prefix='lever-appdata-',dir=evidence) as temp:
            home=Path(temp); game=home/'.minecraft'; world.repeat_statistics(game,assets)
            (game/'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves=game/'saves'; saves.mkdir()
            (saves/'preserve-me.bin').write_bytes(b'unrelated Lever fixture\0\xff')
            authored=None
            for folder,name,played in (('older','Old world',1303000000000),('bravo','',1304000000000),
                                        ('alpha','Lever world',1304000000000)):
                directory=saves/folder; directory.mkdir()
                wire=torch.metadata(world,name,played,69) if folder=='alpha' else world.metadata(name,played,19132)
                (directory/'level.dat').write_bytes(gzip.compress(wire,mtime=0))
                (directory/'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder=='alpha':
                    authored=nbt.Nbt(wire).root()['Data'][1]['Player'][1]
                    torch.inventory(authored,69,2)
            regions=saves/'alpha/region'; regions.mkdir()
            report['saved_input']=authored_regions(m,regions)
            before=world.save_title_files(saves,evidence/'before')
            env=os.environ.copy()
            env.update(APPDATA=str(home),LOCALAPPDATA=str(home),USERPROFILE=str(home),HOME=str(home),
                       ALSOFT_DRIVERS='null',MC_WORLD_RELOAD_SAVES=str(saves),MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence/'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence/'entries'))
            result=world.retained_native_run([str(executable),str(assets),'wall-circuit',str(evidence)],
                env,evidence,saves,before,'lever-wall-circuit',time.time_ns()//1000000,1200)
            print(result.stdout,end=''); print(result.stderr,end='',file=sys.stderr)
            try:
                report['passive_prefix']={'qualification':False,'states':len(state_rows(result.stdout))}
            except (ValueError,KeyError,TypeError) as error:
                report['passive_prefix']={'qualification':False,'decode_error':str(error)}
            require(result.returncode==0,'actual one-opportunity SDK process failed')
            report['receipts']=observations(m,result.stdout)
            witnesses=[s['index'] for s in report['receipts']['states'] if s['redstone_particles']>0]
            report['natural_redstone_particles']={'observed':bool(witnesses),'state_indices':witnesses,
                'scope':'Passive actual DN memberships; no seed, time or emission repair'}
            report['readback']=readback(m,evidence,report['receipts'],authored)
            report['geometry']=geometry_readback(evidence,report['receipts'])
            report['corruption_controls']=corruption_controls(m,result.stdout,evidence,report['receipts'])
            after=world.save_title_files(saves,evidence/'after-verified')
            require(all(after.get(n)==v for n,v in before.items()
                        if not n.startswith('alpha/') or n=='alpha/preserve-chunk.dat'),
                    'unrelated saved bytes unchanged')
            require(all(n.startswith('alpha/') for n in set(after)-set(before)),
                    'no unrelated World creation')
            selected,final=world.snapshot(evidence/'snapshots/lever-saved'),world.snapshot(saves)
            session=str(Path('alpha')/'session.lock')
            report['reselection_disk']={'same_paths':set(final)==set(selected),
                'changed':sorted(n for n in set(final)|set(selected) if final.get(n)!=selected.get(n)),
                'session_key':session,'session_bytes':(saves/session).stat().st_size}
            require(set(final)==set(selected) and all(final[n]==v for n,v in selected.items() if n!=session)
                    and (saves/session).stat().st_size==8,'reselection bytes except actual8byte session token')
        report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)}
        raise
    finally:
        report['inputs_after']={str(p):digest(p) for p in sorted(input_paths(executable))}
        report['assets_after']=world.snapshot(assets)
        if report['inputs_after']!=before_inputs or report['assets_after']!=before_assets:
            report['status']='FAIL'
            report['continuity_error']='Source/product/assets changed'
        (evidence/'run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    require(report['status']=='PASS','retained GUI fixture continuity')
    print('Actual Lever circuit input/render/save/reselection PASS: '+str(evidence))


if __name__=='__main__':
    main()
