"""Author the declared saved inputs. No original/native runtime is invoked."""
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
    """Write case00..case26 into a new output directory; return exact file pins."""
    if declaration is None:declaration=json.loads((HERE/'cases.json').read_text(encoding='utf8'))
    cases=declaration['cases'];fixtures=Path(output);fixtures.mkdir(parents=True,exist_ok=False)
    for c in cases:
        folder=fixtures/c['folder'];(folder/'region').mkdir(parents=True);(folder/'data').mkdir()
        info={'Data':[10,{'RandomSeed':[4,123],'SpawnX':[3,8],'SpawnY':[3,64],'SpawnZ':[3,8],'Time':[4,8],'SizeOnDisk':[4,0],'LastPlayed':[4,0],'LevelName':[8,'Original Piston draw %02d'%c['index']],'version':[3,19132],'rainTime':[3,1000],'raining':[1,0],'thunderTime':[3,1000],'thundering':[1,0]}]}
        (folder/'level.dat').write_bytes(gzip.compress(nbt(info),mtime=0));(folder/'session.lock').write_bytes(bytes(8))
        records=[];header=bytearray(8192);sector=2
        for x,z in declaration['chunks']:
            blocks=bytearray(32768);data=bytearray(16384);height=bytearray([64]*256)
            for xx in range(16):
                for zz in range(16):blocks[(xx<<11)|(zz<<7)|63]=1
            if (x,z)==(0,0):
                at=(8<<11)|(8<<7)|64;blocks[at]=36;data[at//2]=c['metadata'];height[8+16*8]=65
            obj={'Level':[10,{'xPos':[3,x],'zPos':[3,z],'LastUpdate':[4,8],'Blocks':[7,bytes(blocks)],'Data':[7,bytes(data)],'SkyLight':[7,b'\xff'*16384],'BlockLight':[7,bytes(16384)],'HeightMap':[7,bytes(height)],'TerrainPopulated':[1,1],'Entities':[9,[10,[]]],'TileEntities':[9,[10,[]]]}]}
            compressed=zlib.compress(nbt(obj))
            record=struct.pack('>I',len(compressed)+1)+b'\x02'+compressed;n=(len(record)+4095)//4096
            struct.pack_into('>I',header,(x+32*z)*4,(sector<<8)|n);sector+=n;records.append(record+bytes(n*4096-len(record)))
        (folder/'region/r.0.0.mcr').write_bytes(header+b''.join(records))
    return {p.relative_to(fixtures).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(fixtures.rglob('*')) if p.is_file()}

def _pin_bytes(raw):
    return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}

def _inflate(raw,bits):
    decoder=zlib.decompressobj(bits)
    result=decoder.decompress(raw)+decoder.flush()
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('incomplete or trailing compressed NBT stream')
    return result

def payload_pins(output):
    """Pin complete uncompressed NBT bytes, not selected decoded fields."""
    root=Path(output);pins={}
    declaration=json.loads((HERE/'cases.json').read_text(encoding='utf8'))
    for case in declaration['cases']:
        folder=root/case['folder'];level=folder/'level.dat'
        pins[case['folder']+'/level.dat']=_pin_bytes(_inflate(level.read_bytes(),31))
        if (folder/'session.lock').read_bytes()!=bytes(8):
            raise ValueError('authored input SessionLock changed')
        region=(folder/'region/r.0.0.mcr').read_bytes()
        if len(region)!=6*4096 or region[4096:8192]!=bytes(4096):
            raise ValueError('complete authored region extent/timestamp header')
        selected={x+32*z for x,z in declaration['chunks']}
        if any(struct.unpack_from('>I',region,4*i)[0] for i in range(1024) if i not in selected):
            raise ValueError('unexpected authored Chunk location')
        for ordinal,(x,z) in enumerate(declaration['chunks']):
            entry=struct.unpack_from('>I',region,4*(x+32*z))[0]
            if entry!=((ordinal+2)<<8)|1:
                raise ValueError('authored Chunk sector ownership')
            at=(entry>>8)*4096;length=struct.unpack_from('>I',region,at)[0]
            if not 1<length<=4092 or region[at+4]!=2 or any(region[at+4+length:at+4096]):
                raise ValueError('bounded complete authored Chunk record')
            pins[case['folder']+'/region/r.0.0.mcr#%d,%d'%(x,z)]=_pin_bytes(_inflate(region[at+5:at+4+length],15))
    return pins

def audit_worlds(output):
    """Require exact original NBT payloads and report compressed-wire differences."""
    root=Path(output);expected=json.loads((HERE/'authored-worlds.json').read_text(encoding='utf8'))
    actual={p.relative_to(root).as_posix():_pin_bytes(p.read_bytes()) for p in sorted(root.rglob('*')) if p.is_file()}
    dirs=sorted(p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_dir())
    if set(actual)!=set(expected['files']) or dirs!=expected['directories']:
        raise ValueError('complete authored input file/directory census')
    if payload_pins(root)!=expected['payloads']:
        raise ValueError('authored uncompressed NBT bytes differ from original inputs')
    changed=[n for n in actual if actual[n]!=expected['files'][n]]
    return {'payloads_exact':True,'wire_exact':not changed,'wire_different_files':changed,
            'wire_files':actual,'zlib_build':zlib.ZLIB_VERSION,'zlib_runtime':zlib.ZLIB_RUNTIME_VERSION}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    write_worlds(args.output)
    result=audit_worlds(args.output)
    print(json.dumps({'status':'PASS','worlds':27,'files':81,**result},indent=2))
if __name__=='__main__':main()
