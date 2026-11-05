"""Private Windows SDK Item76 placement, natural off transition and save/reselection."""
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
SCENE = ROOT / 'tests/client/gameplay/redstone_torch_scene.json'
PLAN_SHA = 'a0b568c7824d11e1297d49bcf80baa3196bb85a258cf03f112fb50e29e275d2d'
STATE_KEYS = set('index phase world time frames ticks world_ticks focus paused target support plate dust light_count M random E F new_future_target76'.split())


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
    return {name: module('rt_gui_' + name, ROOT / path) for name,path in {
        'world':'tests/tools/client/test_world_selection_loop.py',
        'mining':'tests/tools/client/test_mining_loop.py',
        'farming':'tests/tools/client/test_farming_loop.py',
        'parser':'tests/tools/world/test_source_saving.py',
        'torch':'tests/tools/client/test_torch_placement_loop.py'}.items()}


def read_plan():
    require(digest(SCENE) == PLAN_SHA, 'frozen pre-code scene plan')
    value = json.loads((SCENE).read_text())
    require(all(digest(ROOT/n) == pin for n,pin in value['source_input_sha256'].items()), 'unchanged inherited fixture/producer inputs')
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
    for x,y,z,block,meta in ((3,65,3,1,0),(3,66,3,70,1),(4,64,2,1,0),(4,65,2,55,0)):
        index=at(x,y,z);arrays['Blocks'][index]=block;nibble_set(arrays['Data'],index,meta)
    for x,y,z,light in ((3,64,3,0),(3,65,3,0),(3,66,3,15),(3,67,3,15),(4,64,2,0),(4,65,2,15)):
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
    require(all(actual[c][:-1]==old[c][:-1] for c in old if c!=(0,0)), 'all440 other compressed records byte-exact')
    current=actual[(0,0)][-1]
    require(actual[(0,0)][1:3]==(location,stamp),'selected Chunk slot and timestamp unchanged')
    for cell in read_plan()['cells']:
        x,y,z=cell['xyz'];index=at(x,y,z)
        require(current['Blocks'][1][index]==cell['id'] and nibble(current['Data'][1],index)==cell['metadata'], 'independent declared saved cell decode')
    require(current['BlockLight'][1]==bytes(16384),'all authored BlockLight remains zero')
    for column in read_plan()['saved_column_light_height']:
        x,z=column['xz'];require(current['HeightMap'][1][z*16+x]==column['HeightMap'],'declared saved height')
        for key,start in (('SkyLight_y64_y65',64),('SkyLight_y66_y67',66)):
            if key in column:
                require([nibble(current['SkyLight'][1],at(x,y,z)) for y in (start,start+1)]==column[key],'declared saved SkyLight')
    return {'base':base,'revision':'RT_GUI_WALL_PLATE1_DUST','chunks':441,'changed_chunks':[[0,0]],
            'other_compressed_records_preserved':440,'selected_slot_timestamp_allocation_preserved':True,
            'selected_wire_sha256':hashlib.sha256(wire).hexdigest()}


def unique(pairs):
    value={}
    for key,item in pairs:
        require(key not in value,'duplicate JSON state key');value[key]=item
    return value


def loads(value):
    return json.loads(value,object_pairs_hook=unique,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('non-JSON scalar')))


def integer(value,bits=64):
    return type(value) is int and -(1<<(bits-1))<=value<(1<<(bits-1))


def state_rows(stdout):
    states=[]
    for line in stdout.splitlines():
        if not line.startswith('RT_GUI_STATE\t'):continue
        row=loads(line.split('\t',1)[1])
        require(type(row) is dict and set(row)==STATE_KEYS,'closed passive state schema')
        require(type(row['index']) is int and row['index']==len(states) and row['phase'] in
                ('bootstrap','loading','before','input','off','continuing','saving','reloading','reloaded') and
                type(row['world']) is str and re.fullmatch('[1-9a-f][0-9a-f]*',row['world']) is not None,'ordered actual state identity')
        require(all(integer(row[k]) for k in ('time','frames','ticks','world_ticks','light_count','M')) and
                all(row[k]>=0 for k in ('frames','ticks','world_ticks','light_count')) and
                type(row['focus']) is int and row['focus'] in (0,1) and type(row['paused']) is int and row['paused'] in (0,1) and
                type(row['new_future_target76']) is bool,'typed copied state scalars')
        for key in ('target','support','plate','dust'):
            cell=row[key]
            require(type(cell) is list and len(cell)==3 and all(type(x) is int for x in cell) and
                    0<=cell[0]<=255 and 0<=cell[1]<=15 and 0<=cell[2]<=15,'typed actual cell')
        rng=row['random']
        require(type(rng) is dict and set(rng)=={'seed','gaussian','have'} and
                type(rng['seed']) is str and re.fullmatch('[0-9a-f]{12}',rng['seed']) and
                type(rng['gaussian']) is str and re.fullmatch('[0-9a-f]{16}',rng['gaussian']) and
                type(rng['have']) is bool,'passive Random bits, no seed projection')
        for collection in ('E','F'):
            require(type(row[collection]) is list,'actual scheduler inventory')
            for tick in row[collection]:
                require(type(tick) is list and len(tick)==7 and type(tick[0]) is str and
                        re.fullmatch('[1-9a-f][0-9a-f]*',tick[0]) and
                        all(integer(x,32) for x in tick[1:5]) and all(integer(x) for x in tick[5:]),'full actual scheduled record')
        require(sorted(row['E'])==sorted(row['F']),'same actual E/F record identities and full fields')
        if states:
            require(all(row[k]>=states[-1][k] for k in ('frames','ticks','world_ticks')),'monotone completed native counters')
        states.append(row)
    require(states,'actual returned-frame state observations required')
    return states


def observations(m,stdout):
    farming=m['farming'];states=state_rows(stdout)
    require(all(line.split(None,1)[0].split('\t',1)[0] in {'RT_GUI_STATE','RT_GUI_INPUT','RT_GUI_RETURN','RT_GUI_SAVE',
                'RT_GUI_RELOAD','RT_GUI_FINISH','RT_GUI_GEOMETRY'} for line in stdout.splitlines() if line.startswith('RT_GUI_')),
            'closed authored receipt tags; ordinary constructor stdout remains raw')
    rows={}
    for tag in ('INPUT','RETURN','SAVE','RELOAD','FINISH'):
        found=farming.rows(stdout,'RT_GUI_'+tag);require(len(found)==1,'one complete '+tag+' receipt');rows[tag.lower()]=found[0]
    inp=rows['input']
    expected={'kind':'redstone-torch','taps':'1','right-clicks':'1','block-uses':'1','air-uses':'0','admitted':'1',
              'placed':'1','sounds':'1','count-stores':'1','used':'1','cleared':'0','equip-reset':'1','drained':'1',
              'placed-hooks':'source-control-flow','onAdded-before-face':'source-control-flow','metadata':'0','target':'3,65,2','face':'2'}
    require(set(inp)==set(expected)|{'direct-suffix'} and all(inp[k]==v for k,v in expected.items()) and inp['direct-suffix'] in ('0','1'),'actual one-RMB input suffix')
    ret=rows['return'];save=rows['save'];loaded=rows['reload'];finish=rows['finish']
    require(ret=={'item':'76','block':'75','metadata':'4','count-before':'2','count-after':'1','damage':'0','health':'5',
                  'same-stack':'1','used':'1','hud':'1','held':'1','continuing':'3','future-due':ret.get('future-due')} and
            re.fullmatch('0|-?[1-9][0-9]*',ret['future-due']),'exact returned state')
    require(set(save)==set('sample candidates health support block metadata plate dust time source-save-title pre-frame-owned-nbt items'.split()) and
            all(save[k]==v for k,v in {'health':'5','support':'1','block':'75','metadata':'4','plate':'1','dust':'0',
                'source-save-title':'1','pre-frame-owned-nbt':'1','items':'0'}.items()) and
            save['sample'].isdigit() and save['candidates'].isdigit() and 0<=int(save['sample'])<int(save['candidates']) and
            re.fullmatch('0|-?[1-9][0-9]*',save['time']),'actual selected paused save entry')
    require(loaded=={'distinct-world':'1','distinct-player':'1','health':'5','support':'1','block':'75','metadata':'4',
                    'plate':'1','dust':'0','item':'76','count':'1','damage':'0','selected':'0','hud':'1','held':'1','used':'1','items':'0'},'actual reloaded values')
    require(set(finish)=={'cleanup','windows-absent','checks'} and finish['cleanup']==finish['windows-absent']=='1' and
            finish['checks'].isdigit() and int(finish['checks'])>0,'checked terminal cleanup')
    witnesses=[s for s in states if s['new_future_target76']]
    require(len(witnesses)==1,'one actual first pending target76 deadline witness')
    lit=witnesses[0];due=int(ret['future-due'])
    require(lit['target'][:2]==[76,4] and lit['support'][0]==1 and lit['plate'][:2]==[70,1] and
            lit['focus']==1 and lit['paused']==0 and any(t[1:5]==[3,65,2,76] and t[5]==due and 0<due-lit['time']<=2 for t in lit['E']),
            'actual lit76 and future Source scheduler record')
    off=[s for s in states if s['world']==lit['world'] and s['index']>lit['index'] and s['time']>=due and
         s['target'][:2]==[75,4] and s['plate'][:2]==[70,1] and s['dust'][:2]==[55,0]]
    require(off and off[0]['world_ticks']>lit['world_ticks'],'natural later due off75 under pressed input')
    require(len({s['world'] for s in states})==2,'two actual World identities across reselection')
    geometry=farming.rows(stdout,'RT_GUI_GEOMETRY')
    require(len(geometry)==3 and [x['phase'] for x in geometry]==['before','off','reloaded'],'three actual compiled terrain batches')
    for row in geometry:
        require(set(row)==set('phase vertices bytes arrays mode terrain-returned type2-inclusion'.split()) and
                row['vertices'].isdigit() and int(row['vertices'])>0 and row['bytes'].isdigit() and
                int(row['bytes'])==int(row['vertices'])*32 and row['arrays']=='7' and row['mode']=='4' and
                row['terrain-returned']=='1' and row['type2-inclusion']=='source-control-flow','actual terrain geometry receipt')
    return dict(rows,states=states,geometry=geometry,first_lit=lit['index'],first_off=off[0]['index'])


def readback(m,evidence,receipt,authored):
    parser,mining,farming,torch=[m[k] for k in ('parser','mining','farming','torch')]
    owned=lambda n:parser.Nbt(parser.read(evidence/n)).root()
    before,off=owned('rt-player-before.nbt'),owned('rt-player-off.nbt')
    torch.inventory(before,76,2);torch.inventory(off,76,1)
    mining.same_fields(authored,before,('Pos','Rotation','Health','Score','Dimension','Inventory'),'saved input versus actual loaded Player')
    saved=evidence/'snapshots/rt-saved/alpha'
    player=parser.inflate(parser.read(saved/'level.dat'),31)['Data'][1]['Player'][1]
    entry=owned(f"rt-player-entry-{int(receipt['save']['sample']):04}.nbt")
    reopened=owned('rt-player-reloaded.nbt')
    for value in (player,entry,reopened):torch.inventory(value,76,1)
    mining.same_fields(player,entry,('Pos','Motion','Rotation','Health','Score','Dimension','Inventory'),'actual selected Player save body')
    mining.same_fields(player,reopened,('Health','Score','Dimension','Inventory'),'actual independently reloaded Player')
    info=parser.inflate(parser.read(saved/'level.dat'),31)['Data'][1]
    require(info['Time']==[4,int(receipt['save']['time'])],'independent saved WorldTime')
    level=farming.selected_chunk(parser,saved/'region/r.0.0.mcr')[4]
    for x,y,z,block,meta in ((3,65,2,75,4),(3,65,3,1,0),(3,66,3,70,1),(4,65,2,55,0)):
        at=(x<<11)|(z<<7)|y
        require(level['Blocks'][1][at]==block and nibble(level['Data'][1],at)==meta,'independent saved wall/torch/plate/Dust')
    target=(3<<11)|(2<<7)|65
    require(nibble(level['BlockLight'][1],target)==0,'actual saved off Torch target BlockLight0')
    items,chunks=mining.saved_items(parser,saved);require(not items,'no synthesized/saved Item drops')
    for row in receipt['geometry']:
        wire=(evidence/('rt-geometry-'+row['phase']+'.bin')).read_bytes()
        require(len(wire)==int(row['bytes']),'actual captured native geometry byte extent')
    return {'target':[3,65,2,75,4],'plate':[3,66,3,70,1],'dust':[4,65,2,55,0],
            'saved_time':int(receipt['save']['time']),'Inventory':[[0,76,1,0]],'saved_items':0,'saved_chunks':chunks}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--assets',type=Path,required=True)
    parser.add_argument('--artifacts',type=Path,required=True);args=parser.parse_args()
    require(os.name=='nt','Windows GUI fixture; no implicit platform launch')
    plan=read_plan();m=modules();world,torch,nbt,mining=[m[k] for k in ('world','torch','parser','mining')]
    executable,assets=args.executable.resolve(strict=True),args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True,exist_ok=True)
    evidence=Path(tempfile.mkdtemp(prefix='redstone-torch-powered-',dir=args.artifacts.resolve()))
    for name in ('snapshots','entries'):(evidence/name).mkdir()
    inputs={p for p in HERE.iterdir() if p.is_file()};inputs.add(executable);inputs.add(ROOT/'CMakeLists.txt');inputs.add(SCENE)
    inputs.update(executable.parent.glob('*.dll'))
    for folder in ('src','include','cmake','tests/client','tests/tools/client','tests/tools/world'):
        inputs.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.c','.h','.inc','.py','.cmake'))
    before_inputs={str(p):digest(p) for p in sorted(inputs)};before_assets=world.snapshot(assets)
    report={'status':'FAIL','scope':__doc__,'plan':plan,'inputs_before':before_inputs,'assets_before':before_assets}
    (evidence/'run.json').write_text(json.dumps(report,indent=2)+'\n')
    try:
        with tempfile.TemporaryDirectory(prefix='rt-appdata-',dir=evidence) as temp:
            home=Path(temp);game=home/'.minecraft';world.repeat_statistics(game,assets)
            (game/'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves=game/'saves';saves.mkdir();(saves/'preserve-me.bin').write_bytes(b'unrelated RT fixture\0\xff')
            authored=None
            for folder,name,played in (('older','Old world',1303000000000),('bravo','',1304000000000),('alpha','Redstone Torch world',1304000000000)):
                directory=saves/folder;directory.mkdir()
                wire=torch.metadata(world,name,played,76) if folder=='alpha' else world.metadata(name,played,19132)
                (directory/'level.dat').write_bytes(gzip.compress(wire,mtime=0))
                (directory/'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder=='alpha':authored=nbt.Nbt(wire).root()['Data'][1]['Player'][1];torch.inventory(authored,76,2)
            regions=saves/'alpha/region';regions.mkdir();report['saved_input']=authored_regions(m,regions)
            before=world.save_title_files(saves,evidence/'before')
            env=os.environ.copy();env.update(APPDATA=str(home),LOCALAPPDATA=str(home),USERPROFILE=str(home),HOME=str(home),
                ALSOFT_DRIVERS='null',MC_WORLD_RELOAD_SAVES=str(saves),MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence/'snapshots'),MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence/'entries'))
            result=world.retained_native_run([str(executable),str(assets),'powered-placement',str(evidence)],env,evidence,saves,before,
                'redstone-torch-powered',time.time_ns()//1000000,1200)
            print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
            try:
                prefix=state_rows(result.stdout)
                report['passive_prefix']={'qualification':False,'states':len(prefix),
                    'actual_Plate0_indices':[s['index'] for s in prefix if s['plate'][:2]==[70,0]]}
            except (ValueError,KeyError,TypeError) as error:
                report['passive_prefix']={'qualification':False,'decode_error':str(error)}
            require(result.returncode==0,'actual one-opportunity SDK process failed')
            report['receipts']=observations(m,result.stdout);report['readback']=readback(m,evidence,report['receipts'],authored)
            after=world.save_title_files(saves,evidence/'after-verified')
            require(all(after.get(n)==v for n,v in before.items() if not n.startswith('alpha/') or n=='alpha/preserve-chunk.dat'),'unrelated saved bytes unchanged')
            require(all(n.startswith('alpha/') for n in set(after)-set(before)),'no unrelated World creation')
            selected,final=world.snapshot(evidence/'snapshots/rt-saved'),world.snapshot(saves)
            session=str(Path('alpha')/'session.lock')
            report['reselection_disk']={'same_paths':set(final)==set(selected),'changed':sorted(n for n in set(final)|set(selected) if final.get(n)!=selected.get(n)),
                                       'session_key':session,'session_bytes':(saves/session).stat().st_size}
            require(set(final)==set(selected) and all(final[n]==v for n,v in selected.items() if n!=session) and (saves/session).stat().st_size==8,
                    'reselection disk-byte invariance except actual eight-byte session token')
        report['status']='PASS'
    except BaseException as error:
        report['error']={'type':type(error).__name__,'message':str(error)};raise
    finally:
        report['inputs_after']={str(p):digest(p) for p in sorted(inputs)};report['assets_after']=world.snapshot(assets)
        if report['inputs_after']!=before_inputs or report['assets_after']!=before_assets:
            report['status']='FAIL';report['continuity_error']='Source/product/assets changed'
        (evidence/'run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    require(report['status']=='PASS','retained GUI fixture continuity')
    print('Actual bounded native Item76/powered-off/save/reselection PASS: '+str(evidence))


if __name__=='__main__':main()
