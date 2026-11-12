from pathlib import Path
import argparse,hashlib,json,struct,sys
parser=argparse.ArgumentParser(description='Audit actual captured original strong-owner and ordered RAF traces; no Java/native execution')
parser.add_argument('--capture-dir',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
options=parser.parse_args()
BASE=options.capture_dir.resolve(strict=True)
require_capture=json.loads((BASE/'receipt.json').read_text())
if require_capture['status']!='OBSERVED_AND_AUDITED_ORIGINAL_SOURCE_REGION_OUTPUT_COHORT' or not require_capture['inputs_unchanged']:
    raise ValueError('qualified original capture required')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def require(b,s):
    if not b:raise ValueError(s)
def qidentity(e):return e['qj']['identity']['oid']
result={'status':'FAIL','scope':'Read-only audit of already captured original JDI owner/RAF traces; no new original or native processes','recipe_sha256':sha(Path(__file__)),'hosts':{}}
try:
    for host,cohort in [(require_capture['host_platform'],'.')]:
        directory=BASE/cohort;path=directory/'debugger/checkpoints.ndjson';require(path.stat().st_size<=67108864,'bounded captured trace');before=sha(path);events=[json.loads(x)for x in path.read_text().splitlines()];commits=[e for e in events if e['event']=='entry'and e['location']['class']=='qj'and e['location']['descriptor']=='(II[BI)V'];require(len(commits)==7,'seven actual commit entries')
        bindings=[]
        for entry in commits:
            phase=entry['phase'];hz=[e for e in events if e['event']=='entry'and e['phase']==phase and e['location']['class']=='hz'and e['location']['method']=='close'];require(len(hz)==1,'one actual Hz close for this phase');h=hz[0]['receiver'];require(entry['receiver']['identity']==h['qj']['identity']and entry['arguments'][2]==h['buffer']and entry['arguments'][3]==h['count'],'same strong Hz QJ and exact actual buffer/count operand');require(entry['arguments'][:2]==[h['fields']['hz.b'],h['fields']['hz.c']],'same actual Hz local coordinates');bindings.append({'phase':phase,'hz':h['identity'],'qj':h['qj']['identity'],'raf':h['qj']['raf'],'buffer':h['buffer'],'count':h['count']})
        words=[]
        for entry in events:
            if entry['event']!='entry'or entry['location']['class']!='java.io.RandomAccessFile'or entry['location']['method']!='writeInt':continue
            exits=[e for e in events if e['sequence']>entry['sequence']and e['phase']==entry['phase']and e['event']=='return'and e['location']['class']=='java.io.RandomAccessFile'and e['location']['method']=='writeInt'];require(exits,'actual returning writeInt');end=exits[0]['sequence'];children=[e for e in events if entry['sequence']<e['sequence']<end and e['event']=='entry'and e['location']['class']=='java.io.RandomAccessFile'and e['location']['method']=='write'and e['location']['descriptor']=='(I)V'and len(e['frames'])>1 and e['frames'][1]['class']=='java.io.RandomAccessFile'and e['frames'][1]['method']=='writeInt'];word=entry['arguments'][0];expected=list(struct.pack('>I',word&0xffffffff));require(len(children)==4 and [e['arguments'][0]&255 for e in children]==expected,'actual four ordered big-endian primitive RAF writes');require(all(e['receiver']['identity']==entry['receiver']['identity']for e in children),'same actual RAF for each primitive byte');words.append({'phase':entry['phase'],'word':word,'bytes':expected,'raf':entry['receiver']['identity']})
        require(len(words)==17,'exact source offset/length/timestamp word count')
        ram_stores=[]
        for e in events:
            if e['event']!='source-prefix'or e['location']['class']!='qj'or e['location']['descriptor']!='(III)V'or e['location']['pc']!=12:continue
            args=e['arguments'];slot=str(args[0]+32*args[1]);kind='offsets'if e['location']['method']=='a'else'timestamps';require(e['qj'][kind][slot]==args[2],'actual RAM store is published before lower RAF seek');future=[x for x in events if x['sequence']>e['sequence']and x['event']=='entry'and x['location']['class']=='java.io.RandomAccessFile'and x['location']['method']=='seek'and x['phase']==e['phase']];require(future and future[0]['qj'][kind][slot]==args[2],'next actual RAF seek sees already published RAM');ram_stores.append({'phase':e['phase'],'kind':kind,'slot':slot,'value':args[2],'next_raf_seek':future[0]['arguments'][0]})
        require(len(ram_stores)==11,'five offset stores and six timestamp stores')
        old=commits[-1];require(old['phase']==9 and old['qj']['rafFields']['java.io.RandomAccessFile.closed']is True and old['qj']['fdFields']['java.io.FileDescriptor.closed']is True,'old commit enters same already source-closed RAF');old_fail=[e for e in events if e['phase']==9 and e['event']=='exception'and e['exception']['class']=='java.io.IOException'];require(any(e['catch']['class']=='qj'and e['catch']['pc']==437 for e in old_fail),'actual old RAF source IOException/catch retained')
        closes=[e for e in events if e['phase']==7 and e['stage']=='before-cache-flush'and e['event']=='return'and e['location']['class']=='java.io.RandomAccessFile'and e['location']['method']=='close'];require(len(closes)==2 and all(e['receiver']['fields']['java.io.RandomAccessFile.closed']is True for e in closes),'source cache flush actually closes both cached RAFs');require(sha(path)==before,'captured trace unchanged during audit')
        result['hosts'][host]={'trace_sha256':before,'commit_bindings':bindings,'ordered_primitive_writeInt_calls':len(words),'ordered_primitive_bytes':4*len(words),'source_RAM_store_before_RAF_seek':ram_stores,'source_cache_RAF_closes':2,'old_closed_RAF_commit_source_catch':True}
    result['status']='PASS_ORIGINAL_STRONG_OWNER_AND_ORDERED_RAF_TRACE_AUDIT'
except BaseException as e:result['error']=repr(e)
options.output.parent.mkdir(parents=True,exist_ok=True)
with options.output.open('x',encoding='utf-8',newline='\n') as stream:stream.write(json.dumps(result,indent=2)+'\n')
print(result['status'],result.get('error'));require(result['status']=='PASS_ORIGINAL_STRONG_OWNER_AND_ORDERED_RAF_TRACE_AUDIT','trace audit failed')
