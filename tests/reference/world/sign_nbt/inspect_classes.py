"""Static class/signature/Code pins only; never loads or executes original code."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

JAR_SHA = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def inspect(raw):
    offset = 0
    def take(n):
        nonlocal offset
        result = raw[offset:offset+n]
        if len(result) != n:
            raise ValueError('Truncated class')
        offset += n
        return result
    def u1(): return take(1)[0]
    def u2(): return struct.unpack('>H',take(2))[0]
    def u4(): return struct.unpack('>I',take(4))[0]
    if u4() != 0xcafebabe:
        raise ValueError('Class magic')
    minor, major = u2(),u2()
    pool = [None]*u2()
    i = 1
    while i < len(pool):
        tag = u1()
        if tag == 1:
            # Names/descriptors are ASCII; raw unrelated modified-UTF constants
            # are not decoded as execution inputs or rewritten.
            pool[i] = ('utf8',take(u2()))
        elif tag in (3,4): pool[i]=(tag,take(4))
        elif tag in (5,6): pool[i]=(tag,take(8));i+=1
        elif tag in (7,8,16,19,20): pool[i]=(tag,u2())
        elif tag in (9,10,11,12,17,18): pool[i]=(tag,u2(),u2())
        elif tag == 15: pool[i]=(tag,u1(),u2())
        else: raise ValueError('Pool tag '+str(tag))
        i += 1
    def text(i):
        if pool[i][0] != 'utf8':raise ValueError('UTF8 reference')
        return pool[i][1].decode('ascii')
    def cls(i):
        if i == 0:return None
        if pool[i][0] != 7:raise ValueError('Class reference')
        return text(pool[i][1])
    def attributes():
        result = []
        for _ in range(u2()):
            name = text(u2())
            result.append((name,take(u4())))
        return result
    access,this,parent = u2(),u2(),u2()
    interfaces = [cls(u2()) for _ in range(u2())]
    fields=[]
    for _ in range(u2()):
        f={'access':u2(),'name':text(u2()),'descriptor':text(u2())}
        attributes()
        fields.append(f)
    methods=[]
    for _ in range(u2()):
        m={'access':u2(),'name':text(u2()),'descriptor':text(u2())}
        for name,content in attributes():
            if name == 'Code':
                max_stack,max_locals,length=struct.unpack('>HHI',content[:8])
                code=content[8:8+length]
                if len(code) != length:raise ValueError('Code extent')
                m.update(code_bytes=length,code_sha256=sha(code),
                         code_attribute_sha256=sha(content),max_stack=max_stack,max_locals=max_locals)
        methods.append(m)
    attributes()
    if offset != len(raw):raise ValueError('Trailing class bytes')
    return {'name':cls(this),'superclass':cls(parent),'access':access,
            'interfaces':interfaces,'minor':minor,'major':major,
            'class_bytes':len(raw),'class_sha256':sha(raw),'fields':fields,'methods':methods,
            'referenced_classes':sorted({cls(i) for i,v in enumerate(pool) if v is not None and v[0] == 7})}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    jar=args.jar.resolve(strict=True)
    raw=jar.read_bytes()
    if sha(raw) != JAR_SHA:raise ValueError('Pinned original archive required')
    classes={}
    with zipfile.ZipFile(jar) as archive:
        for name in ('yk','ow','nu','ij','as'):
            classes[name]=inspect(archive.read(name+'.class'))
        # Pin the concrete original tags named directly by NU. Do not recurse
        # into unrelated World/renderer dependencies or execute any initializer.
        names=set(archive.namelist())
        for name in classes['nu']['referenced_classes']:
            if name not in classes and name+'.class' in names:
                classes[name]=inspect(archive.read(name+'.class'))
        if len(classes) > 24:raise ValueError('Bounded direct NBT class inventory')
    output={'schema':1,'scope':'Static original signatures and method Code pins; no target execution.',
            'client_jar_sha256':JAR_SHA,'inspector_sha256':sha(Path(__file__).read_bytes()),
            'classes':classes}
    with args.output.open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(output,stream,indent=2);stream.write('\n')
    print('Static-only PASS: '+str(len(classes))+' original classes; no JVM executed')

if __name__ == '__main__':main()
