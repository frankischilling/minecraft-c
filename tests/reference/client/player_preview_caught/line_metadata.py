"""Read source lines and catch tables from immutable classfile attributes."""
import hashlib
import struct

def inspect(data):
 cursor=0
 def take(n):
  nonlocal cursor
  value=data[cursor:cursor+n]
  if len(value)!=n:raise ValueError('truncated classfile metadata')
  cursor+=n;return value
 def u1():return take(1)[0]
 def u2():return struct.unpack('>H',take(2))[0]
 def u4():return struct.unpack('>I',take(4))[0]
 if u4()!=0xcafebabe:raise ValueError('classfile magic')
 minor,major=u2(),u2();pool=[None]*u2();index=1
 while index<len(pool):
  tag=u1()
  if tag==1:pool[index]=take(u2()).decode('utf8',errors='replace')
  elif tag in (3,4):take(4)
  elif tag in (5,6):take(8);index+=1
  elif tag in (7,8,16,19,20):pool[index]=(tag,u2())
  elif tag in (9,10,11,12,17,18):take(4)
  elif tag==15:take(3)
  else:raise ValueError('constant-pool tag')
  index+=1
 def attributes():return [(pool[u2()],take(u4())) for unused in range(u2())]
 def name(i):return None if i==0 else pool[pool[i][1]]
 u2();this=name(u2());u2();take(u2()*2)
 for unused in range(u2()):u2();u2();u2();attributes()
 methods={}
 for unused in range(u2()):
  access,method,descriptor=u2(),pool[u2()],pool[u2()];meta={'access':access}
  for kind,content in attributes():
   if kind!='Code':continue
   length=struct.unpack_from('>I',content,4)[0];code=content[8:8+length];offset=8+length
   count=struct.unpack_from('>H',content,offset)[0];offset+=2;handlers=[]
   for ignored in range(count):
    start,end,handler,catch=struct.unpack_from('>HHHH',content,offset);offset+=8;handlers.append({'start':start,'end':end,'handler':handler,'catch':name(catch)})
   count=struct.unpack_from('>H',content,offset)[0];offset+=2;lines=[];locals_present=False
   for ignored in range(count):
    attribute,size=struct.unpack_from('>HI',content,offset);offset+=6;payload=content[offset:offset+size];offset+=size
    if pool[attribute]=='LineNumberTable':
     n=struct.unpack_from('>H',payload,0)[0];lines=[{'pc':pc,'line':line} for pc,line in (struct.unpack_from('>HH',payload,2+4*i) for i in range(n))]
    if pool[attribute]=='LocalVariableTable':locals_present=True
   if offset!=len(content):raise ValueError('Code metadata extent')
   meta.update(code_bytes=length,code_sha256=hashlib.sha256(code).hexdigest(),lines=lines,handlers=handlers,local_variable_table=locals_present)
  methods[method+descriptor]=meta
 source=None
 for kind,content in attributes():
  if kind=='SourceFile':source=pool[struct.unpack('>H',content)[0]]
 if cursor!=len(data):raise ValueError('classfile metadata extent')
 return {'class':this,'class_sha256':hashlib.sha256(data).hexdigest(),'major':major,'minor':minor,'source_file':source,'methods':methods}

def source_line(method,pc):
 values=[row for row in method['lines'] if row['pc']<=pc]
 return values[-1]['line'] if values else -1
