"""Offline class/Code inspection only; no bytecode is executed or rewritten."""
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent


def require(value,message):
    if not value: raise ValueError(message)


def read_class(data):
    cursor=0
    def take(n):
        nonlocal cursor
        result=data[cursor:cursor+n]; require(len(result)==n,'class boundary'); cursor+=n; return result
    def u1(): return take(1)[0]
    def u2(): return struct.unpack('>H',take(2))[0]
    def u4(): return struct.unpack('>I',take(4))[0]
    require(u4()==0xcafebabe,'class magic'); minor=u2(); major=u2()
    cp=[None]*u2(); index=1
    while index<len(cp):
        tag=u1()
        if tag==1: raw=take(u2()); cp[index]=(tag,raw.decode('utf-8',errors='replace'),raw.hex())
        elif tag in [3,4]: cp[index]=(tag,take(4))
        elif tag in [5,6]: cp[index]=(tag,take(8)); index+=1
        elif tag in [7,8,16,19,20]: cp[index]=(tag,u2())
        elif tag in [9,10,11,12,17,18]: cp[index]=(tag,u2(),u2())
        elif tag==15: cp[index]=(tag,u1(),u2())
        else: raise ValueError('constant tag '+str(tag))
        index+=1
    def utf(index): require(cp[index][0]==1,'UTF8 ref'); return cp[index][1]
    def resolve(index):
        value=cp[index]; tag=value[0]
        if tag==1: return {'kind':'utf8','text':value[1],'raw_hex':value[2]}
        if tag==7: return {'kind':'class','name':utf(value[1])}
        if tag==8: return {'kind':'String','text':utf(value[1]),'utf8_hex':cp[value[1]][2]}
        if tag==3: return {'kind':'int','value':struct.unpack('>i',value[1])[0]}
        if tag==4: return {'kind':'float','bits':value[1].hex()}
        if tag==5: return {'kind':'long','value':struct.unpack('>q',value[1])[0]}
        if tag==6: return {'kind':'double','bits':value[1].hex()}
        if tag==12: return {'name':utf(value[1]),'descriptor':utf(value[2])}
        if tag in [9,10,11]: return {'kind':{9:'field',10:'method',11:'interface_method'}[tag], 'class':resolve(value[1])['name'],**resolve(value[2])}
        return {'kind':'constant','tag':tag,'indices':list(value[1:])}
    def attributes():
        result=[]
        for _ in range(u2()): name=utf(u2()); raw=take(u4()); result.append((name,raw))
        return result
    access=u2(); this=u2(); super_index=u2(); interfaces=[resolve(u2())['name'] for _ in range(u2())]
    fields=[]
    for _ in range(u2()):
        flags=u2(); name=utf(u2()); descriptor=utf(u2()); attrs=attributes()
        field={'access':flags,'name':name,'descriptor':descriptor,'static':bool(flags&8),'volatile':bool(flags&64),
               'default':'null' if descriptor[0] in '[L' else '+0.0' if descriptor in ['F','D'] else 'false' if descriptor=='Z' else '0'}
        for attr,raw in attrs:
            if attr=='ConstantValue': field['constant']=resolve(struct.unpack('>H',raw)[0])
        fields.append(field)
    methods=[]
    for _ in range(u2()):
        flags=u2(); name=utf(u2()); descriptor=utf(u2()); attrs=attributes()
        method={'access':flags,'name':name,'descriptor':descriptor}
        for attr,raw in attrs:
            if attr!='Code': continue
            length=struct.unpack_from('>I',raw,4)[0]; code=raw[8:8+length]; require(len(code)==length,'Code length')
            exception_offset=8+length; count=struct.unpack_from('>H',raw,exception_offset)[0]
            exceptions=[]
            for i in range(count):
                start,end,target,catch=struct.unpack_from('>HHHH',raw,exception_offset+2+i*8)
                exceptions.append({'from':start,'to':end,'target':target,'catch':None if catch==0 else resolve(catch)['name']})
            method.update(code_bytes=length,code_sha256=hashlib.sha256(code).hexdigest(),code_attribute_sha256=hashlib.sha256(raw).hexdigest(),
                          max_stack=struct.unpack_from('>H',raw,0)[0],max_locals=struct.unpack_from('>H',raw,2)[0],exceptions=exceptions,
                          instructions=decode(code,resolve))
        methods.append(method)
    attributes(); require(cursor==len(data),'complete class boundary')
    return {'class_bytes':len(data),'class_sha256':hashlib.sha256(data).hexdigest(),'access':access,'major':major,'minor':minor,
            'class':resolve(this)['name'],'super':None if super_index==0 else resolve(super_index)['name'],
            'interfaces':interfaces,'fields':fields,'methods':methods}


def decode(code,resolve):
    # JVM operand widths. Names for complete field/call/allocation/order events;
    # other instructions retain their exact opcode and operand bytes.
    widths={**{op:1 for op in [16,18,21,22,23,24,25,54,55,56,57,58,169,188]},
            **{op:2 for op in [17,19,20,132,*range(153,169),*range(178,185),187,189,192,193,198,199]},
            185:4,186:4,197:3,200:4,201:4}
    names={178:'getstatic',179:'putstatic',180:'getfield',181:'putfield',182:'invokevirtual',183:'invokespecial',
           184:'invokestatic',185:'invokeinterface',186:'invokedynamic',187:'new',188:'newarray',189:'anewarray',
           192:'checkcast',193:'instanceof',197:'multianewarray',18:'ldc',19:'ldc_w',20:'ldc2_w',
           198:'ifnull',199:'ifnonnull',177:'return',176:'areturn',172:'ireturn',173:'lreturn',174:'freturn',175:'dreturn',
           191:'athrow',167:'goto',16:'bipush',17:'sipush'}
    result=[]; pc=0
    while pc<len(code):
        start=pc; op=code[pc]; pc+=1
        if op in [170,171]:
            pc+=(4-(pc%4))%4
            if op==170:
                default,low,high=struct.unpack_from('>iii',code,pc); count=high-low+1; require(count>=0,'tableswitch count'); pc+=12+4*count
            else:
                default,count=struct.unpack_from('>ii',code,pc); require(count>=0,'lookupswitch count'); pc+=8+8*count
        elif op==196: pc+=5 if code[pc]==132 else 3
        else: pc+=widths.get(op,0)
        require(pc<=len(code),'instruction boundary')
        args=code[start+1:pc]; record={'pc':start,'opcode':op,'name':names.get(op,'opcode_'+str(op)),'operand_hex':args.hex()}
        if op in [18,19,20,*range(178,187),187,189,192,193,197]:
            index=args[0] if op==18 else struct.unpack_from('>H',args,0)[0]
            record['constant_index']=index; record['reference']=resolve(index)
        result.append(record)
    return result
