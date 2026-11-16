"""Author declared stable saved Piston/Lever inputs; no original/native runtime."""
import argparse,gzip,hashlib,json,struct,zlib
from pathlib import Path
HERE=Path(__file__).resolve().parent

def mutf(text):
    out=bytearray()
    raw=text.encode('utf-16-be','surrogatepass')
    for at in range(0,len(raw),2):
        n=int.from_bytes(raw[at:at+2],'big')
        if 1<=n<128:out.append(n)
        elif n<2048:out.extend((192|(n>>6),128|(n&63)))
        else:out.extend((224|(n>>12),128|((n>>6)&63),128|(n&63)))
    return struct.pack('>H',len(out))+out

def payload(kind,value):
    if kind in (1,2,3,4,5,6):return struct.pack({1:'>b',2:'>h',3:'>i',4:'>q',5:'>I',6:'>Q'}[kind],value)
    if kind==7:return struct.pack('>i',len(value))+bytes(value)
    if kind==8:return mutf(value)
    if kind==9:
        child,items=value;return bytes([child])+struct.pack('>i',len(items))+b''.join(payload(child,item) for item in items)
    if kind==10:return b''.join(bytes([k])+mutf(n)+payload(k,v) for n,(k,v) in value.items())+b'\0'
    raise ValueError('NBT kind')
def nbt(value):return b'\x0a\0\0'+payload(10,value)

def write_worlds(output,declaration=None):
    if declaration is None:declaration=json.loads((HERE/'cases.json').read_text(encoding='utf8'))
    cases=declaration['cases'];fixtures=Path(output);fixtures.mkdir(parents=True,exist_ok=False)
    for c in cases:
        folder=fixtures/c['folder'];(folder/'region').mkdir(parents=True);(folder/'data').mkdir()
        md=declaration['serialized_metadata'];types={'RandomSeed':4,'Time':4,'LastPlayed':4,'SizeOnDisk':4,'LevelName':8,'raining':1,'thundering':1};info={'Data':[10,{k:[types.get(k,3),v] for k,v in md.items()}]}
        (folder/'level.dat').write_bytes(gzip.compress(nbt(info),mtime=0));(folder/'session.lock').write_bytes(bytes(8))
        blocks=bytearray(32768);data=bytearray(16384);height=bytearray([64]*256)
        def cell(x,y,z,id,meta):
            at=(x<<11)|(z<<7)|y;blocks[at]=id;shift=(at&1)*4;data[at//2]=(data[at//2]&~(15<<shift))|(meta<<shift)
            if id and height[z*16+x]<y+1:height[z*16+x]=y+1
        for x in range(16):
            for z in range(16):cell(x,63,z,1,0)
        cell(8,64,1,c['base'],3);cell(7,64,1,69,4);cell(7,64,2,1,0)
        for n in range(c['count']):cell(8,64,2+n,c['carried'],0)
        level={'Level':[10,{'xPos':[3,0],'zPos':[3,0],'LastUpdate':[4,0],'Blocks':[7,bytes(blocks)],'Data':[7,bytes(data)],'SkyLight':[7,b'\xff'*16384],'BlockLight':[7,bytes(16384)],'HeightMap':[7,bytes(height)],'TerrainPopulated':[1,1],'Entities':[9,[10,[]]],'TileEntities':[9,[10,[]]]}]}
        encoded=nbt(level);compressed=zlib.compress(encoded);record=struct.pack('>I',len(compressed)+1)+b'\x02'+compressed;sectors=(len(record)+4095)//4096;header=bytearray(8192);struct.pack_into('>I',header,0,(2<<8)|sectors);struct.pack_into('>I',header,4096,0xfedcba98)
        (folder/'region/r.0.0.mcr').write_bytes(header+record+bytes(sectors*4096-len(record)))
    return {p.relative_to(fixtures).as_posix():_pin(p.read_bytes()) for p in sorted(fixtures.rglob('*')) if p.is_file()}

def _pin(raw):return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def _inflate(raw,bits):
    decoder=zlib.decompressobj(bits);value=decoder.decompress(raw)+decoder.flush()
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:raise ValueError('incomplete/trailing compressed NBT')
    return value
def payload_pins(output):
    root=Path(output);out={}
    for c in json.loads((HERE/'cases.json').read_text(encoding='utf8'))['cases']:
        folder=root/c['folder'];out[c['folder']+'/level.dat']=_pin(_inflate((folder/'level.dat').read_bytes(),31))
        if (folder/'session.lock').read_bytes()!=bytes(8):raise ValueError('initial authored SessionLock')
        raw=(folder/'region/r.0.0.mcr').read_bytes()
        if len(raw)!=12288 or struct.unpack_from('>I',raw,0)[0]!=513 or any(raw[4:4096]) or struct.unpack_from('>I',raw,4096)[0]!=0xfedcba98 or any(raw[4100:8192]):raise ValueError('complete authored region ownership/timestamp')
        n=struct.unpack_from('>I',raw,8192)[0]
        if not 1<n<=4092 or raw[8196]!=2 or any(raw[8196+n:]):raise ValueError('bounded exact authored Chunk record')
        out[c['folder']+'/region/r.0.0.mcr#0,0']=_pin(_inflate(raw[8197:8196+n],15))
    return out
def audit_worlds(output):
    root=Path(output);expected=json.loads((HERE/'authored-worlds.json').read_text(encoding='utf8'))
    actual={p.relative_to(root).as_posix():_pin(p.read_bytes()) for p in sorted(root.rglob('*')) if p.is_file()}
    dirs=sorted(p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_dir())
    if set(actual)!=set(expected['files']) or dirs!=expected['directories']:raise ValueError('complete authored World census')
    if payload_pins(root)!=expected['payloads']:raise ValueError('complete uncompressed NBT payload mismatch')
    changed=[n for n in actual if actual[n]!=expected['files'][n]]
    return {'payloads_exact':True,'wire_exact':not changed,'wire_different_files':changed,'wire_files':actual,'zlib_build':zlib.ZLIB_VERSION,'zlib_runtime':zlib.ZLIB_RUNTIME_VERSION}
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();write_worlds(a.output);print(json.dumps(audit_worlds(a.output),indent=2))
if __name__=='__main__':main()
