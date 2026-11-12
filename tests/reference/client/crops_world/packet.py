"""Validate original Crops operands, query order, Code pins and packed vertices."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
FROZEN = {'CropsObservation.java': 'f2a62607441981a31b9ffcd8ae4eda04aa7619a9e60914a7671a57097fe506e7', 'CropsJdi.java': 'd2ab861f82dd9e566b9b3617dc3b0ff8f8a19fb5b008c47a3d8e5f09cac21ed7', 'profiles.txt': '806df13034df4325276eeb6e7ca29846a63b9ff0293832b05938edddbb06226c', 'declared-inputs.json': '1726a1e751d7cbfa8b9b9ac6a9d2b2847ff24d23d6f6208d9c2ba031e89197ca', 'source-pins.json': '2fba899aa3338fb84d06afd01d76776af0acb9d67671db469f7ef10403f2685f', 'capture_reference.py': '79598d054c379be13ed587b0184eea1fb83cd40bcbc698de3aadd1d45e0b1909'}
CAPTURED = {'selected.txt': 'cdb0243394630f1b7434381d85c925d587e7a214f016479145a6fa646f6733b9', 'events.jsonl': '2134f16f249c83b672a69559093d03f84b8cc31ed89efcea5be2baa7509f2ccc', 'live-code.tsv': '3ffe391f965160b31a03d053902ffbf202e3165708905adc7657da054efeae1d'}
FILES = ('CropsObservation.java','CropsJdi.java','profiles.txt','declared-inputs.json','source-pins.json','capture_reference.py','selected.txt','events.jsonl','live-code.tsv')
NW_KEYS = set('A B h i j k l m n o p q r s t u v w x z'.split())
XYZ = ((8,64,8),(8,64,8),(3,65,3),(8,16777217,8),(8,-16777217,8),(8,2147483647,8),(8,-2147483648,8))
TRANSLATIONS = ((-.25,.5,-.75),(-8,-64,-8),(0,-64,0))
BOXES = ((0,0,0,1,1,1),(.125,.25,.375,.875,.75,.625),(-.25,-.5,-.125,1.5,1.25,1.75))
VERTEX_PCS = tuple(range(145,401,17))+tuple(range(463,719,17))
COUNT = 44


def require(value, label):
    if not value:
        raise ValueError(label)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def lines(path):
    raw = Path(path).read_bytes()
    require(raw.endswith(b'\n') and b'\r' not in raw, 'ASCII LF source records')
    return raw.decode('ascii').splitlines()


def primitive(text):
    result = {}
    for item in text.split(','):
        key, value = item.split('=', 1)
        require(key not in result, 'unique primitive fields')
        result[key] = value
    return result


def dword(value):
    return struct.pack('>d', value).hex()


def double(word):
    require(type(word) is str and re.fullmatch('[0-9a-f]{16}', word), 'raw binary64 operand')
    return struct.unpack('>d', bytes.fromhex(word))[0]


def fword(value):
    return struct.pack('>f', value).hex()


def float_value(word):
    require(type(word) is str and re.fullmatch('[0-9a-f]{8}', word), 'raw binary32 operand')
    return struct.unpack('>f', bytes.fromhex(word))[0]


def raw_nw(state):
    require(set(state) == NW_KEYS, 'complete NW primitive observation')
    for name in 'i j s t u'.split():
        double(state[name])
    for name in 'l m n q w x'.split():
        require(type(state[name]) is bool, 'typed NW boolean')
    for name in NW_KEYS - set('i j s t u l m n q w x'.split()):
        require(type(state[name]) is int, 'typed NW integer')
    require(state['B'] == 2097152 and state['w'] and state['r'] == 7,
            'genuine NW backing and one begun quad batch')
    return {k: str(v).lower() if type(v) is bool else str(v) for k, v in state.items()}


def profiles(directory=HERE):
    result=[]
    for text in lines(Path(directory)/'profiles.txt'):
        parts=text.split(' ')
        require(len(parts)==14 and re.fullmatch('[a-z0-9-]+',parts[0]),'profile framing')
        require(all(re.fullmatch(r'-?(?:0|[1-9][0-9]*)',v) for v in parts[1:]),'signed integer operands')
        p=list(map(int,parts[1:]))
        require(all(-2**31<=v<2**31 for v in p) and (p[12]==1 or p[0] in range(16)) and p[1] in range(3) and p[4] in range(3) and p[5] in range(4) and p[6] in (0,1) and p[8] in range(7) and p[9] in (0,1) and p[10] in (0,1) and p[11] in range(4) and p[12] in (0,1),'closed authored operands')
        result.append({'index':len(result),'name':parts[0],'values':p})
    require(len(result)==COUNT and len({p['name'] for p in result})==COUNT,'complete frozen profile inventory')
    declared=load(Path(directory)/'declared-inputs.json')
    require(declared['profiles']==COUNT and declared['rows']==[{'name':p['name'],'values':p['values']} for p in result],'declared inputs equal executed profiles')
    require([p['values'][0] for p in result[:16]]==list(range(16)),'all world metadata nibbles')
    return result


def fails(p):
    return p[5] in (2,3) or not p[12] and p[5]==1


def wrap(value):
    return (value+2**31)%2**32-2**31


def validate_rows(selected,ps):
    roles=collections.defaultdict(dict);lights=None;guards=[]
    for text in selected:
        row=text.split('\t')
        if row[0]=='CROPS_LIGHT':
            require(lights is None and len(row)==3 and row[1]=='3','actual Dimension light inventory')
            lights=row[2].split(',');require(len(lights)==16 and all(re.fullmatch('[0-9a-f]{8}',x) for x in lights),'sixteen light words')
        elif row[0]=='CROPS_STATIC':
            require(row==['CROPS_STATIC','NW.b=true','NW.c=false','UU.s.length=256','UU.s.final=true'],'genuine NW and emission backing')
        elif row[0] in ('CROPS','CROPS_INPUT','CROPS_BACKING'):
            index=int(row[1]);require(row[0] not in roles[index],'unique original role');roles[index][row[0]]=row
        else:guards.append(text)
    require(set(roles)==set(range(COUNT)) and lights is not None,'all original profiles present')
    result=[];end=0;unchanged=0
    for profile in ps:
        i,p=profile['index'],profile['values'];r=roles[i]
        require(set(r)=={'CROPS','CROPS_INPUT','CROPS_BACKING'},'complete original roles')
        row,inp,backing=(r[k] for k in ('CROPS','CROPS_INPUT','CROPS_BACKING'))
        bm=wrap(88+p[3]);emission=0 if p[7]==-2**31 else p[7];xyz=list(XYZ[p[8]])
        require(inp==['CROPS_INPUT',str(i),'59','88',str(p[3]),str(bm),'0',str(emission),','.join(map(str,p)),','.join(map(str,xyz)),'0'],'actual Crops registry, texture, emission, coordinates and pass')
        require(len(row)==16 and row[:4]==['CROPS',str(i),profile['name'],str(p[0])] and row[4:6]==(['UNRETURNED','NULL'] if fails(p) else ['VOID' if p[12] else 'true','NONE']),'actual original return or NULL prefix')
        before,after=map(int,row[6:8]);delta=0 if fails(p) else 384
        require(before==end and after==before+delta and row[8]=='2097152','one original batch and eight quads')
        require(row[10].split(',')==[dword(x) for x in TRANSLATIONS[p[8] if p[8]<3 else 0]] and row[11].split(',')==[dword(x) for x in BOXES[p[4]]],'actual declared translation and retained bounds')
        old,new=map(primitive,row[13:15]);raw_nw({k:(v=='true' if v in ('true','false') else v if k in 'ijst u'.replace(' ','') else int(v)) for k,v in old.items()})
        require(set(old)==set(new)==NW_KEYS and int(old['o'])==before and int(new['o'])==after,'complete NW before/after fields')
        require(int(new['h'])-int(old['h'])==delta//8 and int(new['p'])-int(old['p'])==delta//12,'32 source/48 packed vertex inventory')
        require(not fails(p) or old==new,'NULL source prefix retains NW state')
        words=row[15].split(',');require(len(words)==384 and all(re.fullmatch('[0-9a-f]{8}',x) for x in words),'full active words or actual inactive tail')
        require(len(backing)==6 and backing[:3]==['CROPS_BACKING',str(i),str(2097152-delta)],'own-prior whole inactive backing')
        result.append({'profile':profile,'raw':'\t'.join(row),'input':'\t'.join(inp),'before':old,'after':new,'words':words,'delta':delta,'lights':lights,'emission':emission,'xyz':xyz,'bm':bm})
        end=after;unchanged+=2097152-delta
    require(guards==['CROPS_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=3','CROPS_GUARD controls network=2 native=1 denied-writes=1 execs=1',f'CROPS_COMPLETE rows={COUNT} own-unchanged-words={unchanged} genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0'],'original source completion and denial controls')
    require(end==15744 and unchanged==92258944,'exact finite geometry/backing inventory')
    return result


def validate_events(events,rows,pins,live):
    methods={cl+'.'+m['name']+m['descriptor']:m for cl,state in pins['classes'].items() for m in state['methods']}
    pinned=set()
    for line in live:
        tag,cl,name,desc,length,code=line.split('\t');key=cl+'.'+name+desc
        require(tag=='CW_LIVE_CODE' and key not in pinned and key in methods and methods[key]['code_bytes']==int(length) and methods[key]['code_sha256']==code,'live reached Code equals original archive')
        pinned.add(key)
    def at(text):
        key,pc=text.rsplit('@',1);require(key in pinned,'reached pinned method');return key,int(pc)
    stack=[];entered={};roots=set();ended=set();failed=set();counts=collections.Counter();vertices=collections.defaultdict(list);outer=collections.Counter();color=set();metadata={};brightness={};cache_brightness={}
    for seq,event in enumerate(events):
        require(type(event['seq']) is int and event['seq']==seq and type(event['profile']) is int and event['profile'] in range(COUNT),'ordered event sequence')
        i=event['profile'];r=rows[i];p=r['profile']['values'];key,pc=at(event['at']);nw=raw_nw(event['nw']);kind=event['event']
        if kind=='entry':
            call=event['call'];require(type(call) is int and call==len(entered),'ordered actual calls')
            counts[key]+=1
            if not stack:
                expected='cv.b(Luu;IDDD)V' if p[12] else 'cv.j(Luu;III)Z'
                args=[None if p[5]>=2 else 'ref:observedBlock']
                args+=([p[0],dword(r['xyz'][0]+.125),dword(r['xyz'][1]+.375),dword(r['xyz'][2]-.25)] if p[12] else r['xyz'])
                require(key==expected and i not in roots and i==len(roots) and event['args']==args and nw==r['before'],'actual declared root invocation')
                roots.add(i)
            else:
                parent=entered[stack[-1]];caller,caller_pc=at(event['caller'])
                require(parent['profile']==i and caller==parent['at'].rsplit('@',1)[0],'proper nested source caller')
                if key.startswith('nw.'):
                    require(event['receiver']=='ref:observedTess','actual singleton NW receiver')
                if key=='cv.b(Luu;IDDD)V':
                    y=float_value(fword(float_value(fword(r['xyz'][1]))-.0625))
                    require(caller=='cv.j(Luu;III)Z' and caller_pc==56 and event['args']==['ref:observedBlock',metadata[i],dword(r['xyz'][0]),dword(y),dword(r['xyz'][2])],'actual i2f/fsub/f2d wrapper origin')
                elif key.startswith('ew.'):
                    require(event['receiver']=='ref:observedCache' and event['args'][:3]==r['xyz'],'genuine EW owner and coordinates')
                    if key=='ew.e(III)I':require(caller=='cv.j(Luu;III)Z' and caller_pc==40 and i in color,'metadata follows color')
                    if key=='ew.a(IIII)F':require(caller=='uu.d(Lxp;III)F' and caller_pc==13 and event['args'][3]==r['emission'],'actual emission before brightness query')
                elif key=='uu.d(Lxp;III)F':
                    require(caller_pc==14 and event['args']==[None if p[5] in (1,3) else 'ref:observedCache',*r['xyz']],'inherited brightness precedes metadata')
                elif key=='ni.a(II)I':
                    require(caller=='cv.b(Luu;IDDD)V' and caller_pc==8 and event['args']==[0,p[0] if p[12] else metadata[i]],'metadata texture getter before override')
                elif key=='nw.a(FFF)V':
                    require(caller=='cv.j(Luu;III)Z' and caller_pc==27 and event['args']==[brightness[i]]*3,'actual gray brightness color');color.add(i)
                elif key=='nw.a(DDDDD)V':
                    n=outer[i];require(caller=='cv.b(Luu;IDDD)V' and n<32 and caller_pc==VERTEX_PCS[n],'32 exact source vertex sites');outer[i]+=1
                elif key=='nw.a(DDD)V':
                    xyz=[fword(double(a)+double(event['nw'][n])) for a,n in zip(event['args'],'stu')]
                    uv=[fword(double(event['nw'][n])) for n in 'ij']
                    require(event['nw']['l'] and event['nw']['m'] and not event['nw']['n'],'actual UV/color/normal flags')
                    vertices[i].append(xyz+uv+[f'{event["nw"]["k"]&0xffffffff:08x}'])
            entered[call]=event;stack.append(call)
        elif kind=='return':
            require(stack and event['call']==stack[-1],'proper returned occurrence');original=entered[stack.pop()]
            require(original['at'].rsplit('@',1)[0]==key,'same returned method')
            if not stack:
                require(event['result']==('void' if p[12] else True) and nw==r['after'],'actual root return and retained state');ended.add(i)
            if key=='ew.e(III)I':
                require(event['result']==(0 if p[8]>=3 else p[0]),'actual metadata nibble');metadata[i]=event['result']
            elif key=='ew.a(IIII)F':cache_brightness[i]=event['result']
            elif key=='uu.d(Lxp;III)F':
                require(event['result']==cache_brightness[i],'brightness child return preserved');brightness[i]=event['result']
            elif key=='ni.a(II)I':
                m=p[0] if p[12] else metadata[i]
                require(event['result']==wrap(r['bm']+(7 if m<0 else m)),'actual signed NI iadd texture return')
        elif kind=='field-read':
            require(stack,'reached source static capture')
            if event['field']=='nw.a':require(key in ('cv.j(Luu;III)Z','cv.b(Luu;IDDD)V') and pc==0 and event['value']=='ref:observedTess','actual singleton capture')
            else:require((key,pc,event['field'],event['value'],event['element'])==('uu.d(Lxp;III)F',5,'uu.s','ref:observedEmissions',r['emission']),'actual final emission array element')
        elif kind=='exception':
            expected=('cv.b(Luu;IDDD)V',8) if p[12] else ('uu.d(Lxp;III)F',13) if p[5]==1 else ('cv.j(Luu;III)Z',14)
            require(stack and fails(p) and event['type']=='java.lang.NullPointerException' and (key,pc)==expected and nw==r['after'] and event['frames'][0]==event['at'] and event['catch'].startswith('CropsObservation.main([Ljava/lang/String;)V@'),'exact original NULL prefix')
            stack.clear();failed.add(i);ended.add(i)
        else:require(False,'known event kind')
    require(not stack and roots==ended==set(range(COUNT)) and failed=={40,41,42},'complete source roots and exceptions')
    for i,r in enumerate(rows):
        require(len(vertices[i])==r['delta']//12 and outer[i]==r['delta']//12,'complete source/packed vertices')
        packed=[]
        for start in range(0,len(vertices[i]),4):
            packed.extend(word for n in (0,1,2,0,2,3) for word in vertices[i][start+n]+['00000000','00000000'])
        require(packed==r['words'][:r['delta']],'all active words derive from observed original UV/vertex arguments')
    return {'events':len(events),'entries':len(entered),'queries':sum(n for k,n in counts.items() if k.startswith('ew.')),'methods':len(pinned),'counts':dict(counts)}


def read_packet(directory=HERE):
    directory=Path(directory);provenance=load(directory/'provenance.json')
    require(type(provenance['profiles']) is int and provenance['profiles']==COUNT and provenance['client_jar_sha256']==JAR and provenance['original_inputs_unchanged'] is True and provenance['original_helpers_unchanged'] is True and type(provenance['source_writes_from_jdi']) is int and provenance['source_writes_from_jdi']==0,'unchanged executed original cohort')
    require(FROZEN and provenance['frozen_authored_files_sha256']==FROZEN and provenance['files_sha256']=={**FROZEN,**CAPTURED},'frozen source/product pins')
    for name,pin in provenance['files_sha256'].items():require(digest(directory/name)==pin,'changed source/product: '+name)
    ps=profiles(directory);observed=validate_rows(lines(directory/'selected.txt'),ps)
    events=[json.loads(line,object_pairs_hook=unique) for line in lines(directory/'events.jsonl')]
    pins=load(directory/'source-pins.json');live=lines(directory/'live-code.tsv');metrics=validate_events(events,observed,pins,live)
    expected_counts={'events':metrics['events'],'queries':metrics['queries'],
                     'field_reads':sum(e['event']=='field-read' for e in events),
                     'exceptions':sum(e['event']=='exception' for e in events),
                     'live_methods':metrics['methods'],'world_wrapper_calls':34,
                     'direct_primitive_calls':10,'root_calls':COUNT,'nw_vertex_calls':1312}
    require(all(type(provenance['source_observations'].get(key)) is int and
                provenance['source_observations'][key]==value for key,value in expected_counts.items()),
            'provenance counts equal actual source observations')
    raw=provenance['raw_stdout_sha256']
    require(type(raw) is list and len(raw)==3 and len(set(raw))==1 and
            all(type(pin) is str and re.fullmatch('[0-9a-f]{64}',pin) for pin in raw),
            'three identical ordinary and read-only stdout fingerprints')
    return {'profiles':ps,'observed':observed,'events':events,'pins':pins,'live':live,'metrics':metrics,'rows':[r['raw'] for r in observed],'inputs':[r['input'] for r in observed],'provenance':provenance}


def projected_cases(placement,directory=HERE):
    require(placement in (1,2),'section or loaded projection');packet=read_packet(directory)
    result=[{'name':r['profile']['name'],'id':59,'metadata':r['profile']['values'][0],'pass':0,'xyz':r['xyz'],'vertices':r['delta']//8,'words':r['words'][:r['delta']]} for r in packet['observed'] if r['profile']['values'][8]==placement]
    require(len(result)==4 and [r['metadata'] for r in result]==[0,7,8,15],'four captured Crops stages')
    return result
