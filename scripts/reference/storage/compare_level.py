#!/usr/bin/env python3
"""Observe paired original level metadata codecs and handlers on fresh files.

C comparisons cover the pure codec. Original handler fallback, file rotation and
session locks are retained separately from native world-owner comparisons.
"""
from __future__ import annotations

from dataclasses import asdict
from pathlib import Path
from datetime import datetime, timezone
import argparse
import base64
import copy
import hashlib
import json
import os
import platform
import re
import shutil
import stat
import struct
import sys
import time
import types
import uuid
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[3]
PIN = {"client":"af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c",
       "server":"033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d"}
CODECS = ("defaults","full","bool-zero","wrong-seed","wrong-int","wrong-name","wrong-bool","wrong-player","wrong-dimension","missing-data")
HANDLERS = ("primary","primary-absent","primary-wrong-type","primary-corrupt","missing-data","wrong-data","both-absent","both-bad","new-only","false-flag","true-flag","conflict","missing-lock","short-lock","trailing-lock","old-lock")
JAVA_ENVIRONMENT = ("JAVA_TOOL_OPTIONS","JDK_JAVA_OPTIONS","_JAVA_OPTIONS","JDK_JAVAC_OPTIONS","CLASSPATH")
MAX_RECORD = 2*1024*1024
MAX_FILE = 65536
MAX_CAPTURE = 32*1024*1024
MARKER = b"fresh metadata reference fixture\n"

class Failure(Exception):
    pass

def require(value,message):
    if not value:raise Failure(message)

def fingerprint(raw):return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def file_record(path):return {'path':str(path),**fingerprint(Path(path).read_bytes())}
def write_json(path,value):path.write_text(json.dumps(value,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
def u16(text):return text.encode('utf-16-be','surrogatepass').hex()
def tag(kind,name,value):return {'type':kind,'name_utf16':u16(name),'value':value}
def compound(name,children):return tag(10,name,children)
def integer(name,value):return tag(3,name,value)
def long(name,value):return tag(4,name,value)
def string(name,text):return tag(8,name,u16(text))
def modified(hex_units):
    out=bytearray()
    for (v,) in struct.iter_unpack('>H',bytes.fromhex(hex_units)):
        if 1<=v<128:out.append(v)
        elif v<2048:out.extend((0xc0|(v>>6),0x80|(v&63)))
        else:out.extend((0xe0|(v>>12),0x80|((v>>6)&63),0x80|(v&63)))
    require(len(out)<=65535,'bounded Modified UTF input');return struct.pack('>H',len(out))+out
def encode(value,named=True):
    t,v=value['type'],value['value'];out=bytearray(bytes([t])+modified(value['name_utf16']) if named else b'')
    if t in (1,2,3,4):out+=struct.pack({1:'>b',2:'>h',3:'>i',4:'>q'}[t],v)
    elif t in (5,6):out+=bytes.fromhex(v)
    elif t==7:out+=struct.pack('>i',len(bytes.fromhex(v)))+bytes.fromhex(v)
    elif t==8:out+=modified(v)
    elif t==9:
        out+=struct.pack('>Bi',v['element_type'],len(v['items']))
        for item in v['items']:out+=encode(item,False)
    elif t==10:
        for item in v:out+=encode(item)
        out.append(0)
    else:raise AssertionError('known fixture type')
    return bytes(out)
def gzip(raw):
    c=zlib.compressobj(6,zlib.DEFLATED,-15);body=c.compress(raw)+c.flush()
    return bytes.fromhex('1f8b08000000000000ff')+body+struct.pack('<II',zlib.crc32(raw),len(raw))

class Reader:
    def __init__(self,raw):self.raw=raw;self.p=0
    def take(self,n):require(0<=n<=len(self.raw)-self.p,'complete NBT');v=self.raw[self.p:self.p+n];self.p+=n;return v
    def number(self,f):return struct.unpack(f,self.take(struct.calcsize(f)))[0]
    def text(self):
        raw=self.take(self.number('>H'));out=[];p=0
        while p<len(raw):
            first=raw[p];p+=1
            if first<128:value=first
            elif first&0xe0==0xc0:
                require(p<len(raw) and raw[p]&0xc0==0x80,'MUTF continuation');value=((first&31)<<6)|(raw[p]&63);p+=1
            else:
                require(first&0xf0==0xe0 and p+1<len(raw) and raw[p]&0xc0==0x80 and raw[p+1]&0xc0==0x80,'MUTF three bytes');value=((first&15)<<12)|((raw[p]&63)<<6)|(raw[p+1]&63);p+=2
            out.append(value)
        return b''.join(struct.pack('>H',x) for x in out).hex()
    def tag(self,kind=None,named=True,depth=0):
        require(depth<=64,"bounded NBT depth")
        t=self.number('>B') if kind is None else kind;name=self.text() if named else '';require(1<=t<=10,'valid named value type')
        if t in (1,2,3,4):value=self.number({1:'>b',2:'>h',3:'>i',4:'>q'}[t])
        elif t in (5,6):value=self.take(4 if t==5 else 8).hex()
        elif t==7:
            n=self.number('>i');require(0<=n<=65536,'byte array bound');value=self.take(n).hex()
        elif t==8:value=self.text()
        elif t==9:
            element=self.number('>B');n=self.number('>i');require(0<=n<=1024,'list bound');value={'element_type':element,'items':[self.tag(element,False,depth+1) for _ in range(n)]}
        else:
            value=[]
            while True:
                element=self.number('>B')
                if element==0:break
                value.append(self.tag(element,True,depth+1))
            require(len({x["name_utf16"] for x in value})==len(value),"unique compound names")
        return {'type':t,'name_utf16':name,'value':value}
def parse_file(path):
    packed=path.read_bytes();require(len(packed)<=65536,'bounded captured metadata');d=zlib.decompressobj(31);raw=d.decompress(packed,65537);require(d.eof and not d.unused_data and not d.unconsumed_tail and len(raw)<=65536,'one complete bounded original gzip member');r=Reader(raw);v=r.tag();require(r.p==len(raw) and v['type']==10,'one complete original named root compound');return v
def children(value):return {x['name_utf16']:x for x in value['value']}
def child(value,name):return children(value).get(u16(name))
def normalized(value):
    out=copy.deepcopy(value);t=out['type']
    if t==10:out['value']=sorted((normalized(x) for x in out['value']),key=lambda x:x['name_utf16'])
    elif t==9:
        out['value']['items']=[normalized(x) for x in out['value']['items']]
        if not out['value']['items']:out['value']['element_type']=1
    elif t==5 and int(out['value'],16)&0x7f800000==0x7f800000 and int(out['value'],16)&0x7fffff:out['value']='7fc00000'
    elif t==6 and int(out['value'],16)&0x7ff0000000000000==0x7ff0000000000000 and int(out['value'],16)&0xfffffffffffff:out['value']='7ff8000000000000'
    return out
def clock(row,value):require(min(row['clock_before'],row['clock_after'])<=value<=max(row['clock_before'],row['clock_after']),'actual sampled LastPlayed/token inside recorded clock bounds')
def signed(hex_value):return int.from_bytes(bytes.fromhex(hex_value),'big',signed=True)

def class_members(raw):
    r=Reader(raw);require(r.take(4)==b'\xca\xfe\xba\xbe','original class magic');r.take(4);n=r.number('>H');cp=[None]*n;i=1
    while i<n:
        t=r.number('>B')
        if t==1:cp[i]=r.take(r.number('>H')).decode('utf-8',errors='replace')
        elif t in (3,4):r.take(4)
        elif t in (5,6):r.take(8);i+=1
        elif t in (7,8,16,19,20):r.take(2)
        elif t in (9,10,11,12,17,18):r.take(4)
        elif t==15:r.take(3)
        else:raise AssertionError('known original constant pool type')
        i+=1
    r.take(6);r.take(2*r.number('>H'))
    def attrs():
        out={}
        for _ in range(r.number('>H')):name=cp[r.number('>H')];out[name]=r.take(r.number('>I'))
        return out
    fields={}
    for _ in range(r.number('>H')):
        mods,name,desc=r.number('>H'),cp[r.number('>H')],cp[r.number('>H')];attrs();fields[(name,desc)]=mods
    methods={}
    for _ in range(r.number('>H')):
        mods,name,desc=r.number('>H'),cp[r.number('>H')],cp[r.number('>H')];a=attrs();code=None
        if 'Code' in a:
            c=Reader(a['Code']);c.take(4);code=c.take(c.number('>I'))
        methods[(name,desc)]={'modifiers':mods,'code':code}
    return fields,methods

def expected_info(data):
    def get(name,default=0):x=child(data,name);return default if x is None else x['value']
    player=child(data,'Player')
    return {'random_seed_hex':f'{get("RandomSeed")&((1<<64)-1):016x}','spawn':[get('SpawnX'),get('SpawnY'),get('SpawnZ')],
        'time_hex':f'{get("Time")&((1<<64)-1):016x}','last_played_hex':f'{get("LastPlayed")&((1<<64)-1):016x}',
        'size_on_disk_hex':f'{get("SizeOnDisk")&((1<<64)-1):016x}','player_present':player is not None,'dimension':0 if player is None or child(player,'Dimension') is None else child(player,'Dimension')['value'],
        'level_name_utf16':get('LevelName',''),'version':get('version'),'raining':get('raining')!=0,'rain_time':get('rainTime'),'thundering':get('thundering')!=0,'thunder_time':get('thunderTime')}

NAME='A\x00\ud800B\udc00\U0001f600'
PLAYER=compound('Player',[integer('Dimension',-1),compound('Opaque',[string('Text',NAME),tag(7,'Bytes','00ff80'),tag(9,'Numbers',{'element_type':3,'items':[integer('',17),integer('',-5)]}),tag(9,'Empty',{'element_type':10,'items':[]}),tag(5,'FloatNaN','7fa12345')])])
FULL=compound('Data',[long('RandomSeed',-(1<<63)+23),integer('SpawnX',-(1<<31)),integer('SpawnY',-7),integer('SpawnZ',(1<<31)-1),long('Time',-1),long('LastPlayed',-12345),long('SizeOnDisk',1234567890123),string('LevelName',NAME),integer('version',1234),integer('rainTime',-(1<<31)),tag(1,'raining',-128),integer('thunderTime',(1<<31)-1),tag(1,'thundering',2),PLAYER,tag(2,'UnknownData',-12)])
def rooted(data=FULL):return compound('',[copy.deepcopy(data),string('UnknownRoot','retain source separately')])
def replace(data,name,replacement):
    out=copy.deepcopy(data);out['value']=[replacement if x['name_utf16']==u16(name) else x for x in out['value']];return out

def same_json(left, right):
    return json.dumps(left, sort_keys=True, separators=(',', ':'), allow_nan=False) == json.dumps(right, sort_keys=True, separators=(',', ':'), allow_nan=False)


def strict_json(data):
    def pairs(items):
        result = {}
        for name, value in items:
            require(name not in result, 'duplicate JSON key')
            result[name] = value
        return result
    try:
        return json.loads(data, object_pairs_hook=pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(Failure('nonfinite JSON')))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise Failure('invalid JSON: ' + str(error)) from error


def keys(value, expected, context):
    require(type(value) is dict and set(value) == set(expected), context + ': complete keys')


def integer_value(value, low, high, context):
    require(type(value) is int and low <= value <= high, context + ': integer bounds')


def hex_value(value, size, context):
    require(type(value) is str and len(value) == size * 2 and re.fullmatch(r'[0-9a-f]*', value), context + ': exact lowercase hex')


def fingerprint_record(value, *, path=False):
    keys(value, ('path', 'bytes', 'sha256') if path else ('bytes', 'sha256'), 'artifact fingerprint')
    integer_value(value['bytes'], 0, 512*1024*1024, 'artifact byte count')
    hex_value(value['sha256'], 32, 'artifact SHA256')
    if path:
        require(type(value['path']) is str and 1 <= len(value['path']) <= 4096, 'artifact path')
    return {name: value[name] for name in ('bytes', 'sha256')}


def order_only(value):
    out = copy.deepcopy(value)
    if out['type'] == 10:
        out['value'] = sorted((order_only(x) for x in out['value']), key=lambda x: x['name_utf16'])
    elif out['type'] == 9:
        out['value']['items'] = [order_only(x) for x in out['value']['items']]
    return out


def parse_raw(raw):
    require(type(raw) is bytes and len(raw) <= MAX_FILE, 'bounded raw metadata')
    reader = Reader(raw)
    result = reader.tag()
    require(result['type'] == 10 and reader.p == len(raw), 'one complete root compound')
    return result


def info_schema(value):
    keys(value, ('random_seed_hex', 'spawn', 'time_hex', 'last_played_hex', 'size_on_disk_hex',
                 'player_present', 'dimension', 'level_name_utf16', 'version', 'raining',
                 'rain_time', 'thundering', 'thunder_time'), 'metadata fields')
    for name in ('random_seed_hex', 'time_hex', 'last_played_hex', 'size_on_disk_hex'):
        hex_value(value[name], 8, name)
    require(type(value['spawn']) is list and len(value['spawn']) == 3, 'three spawn coordinates')
    for item in value['spawn']:
        integer_value(item, -(1 << 31), (1 << 31)-1, 'spawn')
    for name in ('dimension', 'version', 'rain_time', 'thunder_time'):
        integer_value(value[name], -(1 << 31), (1 << 31)-1, name)
    for name in ('raining', 'thundering', 'player_present'):
        require(type(value[name]) is bool, 'typed metadata boolean')
    name = value['level_name_utf16']
    require(type(name) is str and len(name) <= 65535*4 and len(name) % 4 == 0 and re.fullmatch(r'[0-9a-f]*', name) is not None, 'raw UTF16 code units')


def clock_schema(event, extra=()):
    keys(event, ('clock_before', 'clock_after', *extra), 'clock event')
    for key in ('clock_before', 'clock_after'):
        integer_value(event[key], -(1 << 63), (1 << 63)-1, 'sampled clock')
    require(abs(event['clock_after']-event['clock_before']) <= 90000, 'bounded observed clock interval')


def binding_templates(side):
    c, i, h, b, u = ('nu', 'ei', 'mx', 'fm', 'as') if side == 'client' else ('iq', 'ct', 'ie', 'dq', 'ag')
    def m(owner, name, signature, modifiers=1):
        return {'class': owner, 'method': name, 'signature': signature, 'modifiers': modifiers}
    return {'compound_ctor': m(c, '<init>', '()V'), 'info_ctor': m(i, '<init>', '(L'+c+';)V'),
            'handler_ctor': m(h, '<init>', '(Ljava/io/File;Ljava/lang/String;Z)V'),
            'get_compound': m(c, 'k', '(Ljava/lang/String;)L'+c+';'),
            'put_compound': m(c, 'a', '(Ljava/lang/String;L'+c+';)V'),
            'read_gzip': m(u, 'a', '(Ljava/io/InputStream;)L'+c+';', 9),
            'write_gzip': m(u, 'a', '(L'+c+';Ljava/io/OutputStream;)V', 9),
            'info_retained': m(i, 'a', '()L'+c+';'), 'info_list': m(i, 'a', '(Ljava/util/List;)L'+c+';'),
            'handler_load': m(b, 'c', '()L'+i+';'), 'handler_save_retained': m(b, 'a', '(L'+i+';)V'),
            'handler_save_list': m(h, 'a', '(L'+i+';Ljava/util/List;)V'), 'handler_check_lock': m(b, 'b', '()V')}


def field_templates(side):
    c, i, b = ('nu', 'ei', 'fm') if side == 'client' else ('iq', 'ct', 'dq')
    descriptors = ('J', 'I', 'I', 'I', 'J', 'J', 'J', 'L'+c+';', 'I', 'Ljava/lang/String;', 'I', 'Z', 'I', 'Z', 'I')
    fields = {'info.'+name: {'class': i, 'name': name, 'descriptor': descriptor, 'modifiers': 2}
              for name, descriptor in zip('abcdefghijklmno', descriptors)}
    fields['handler.e'] = {'class': b, 'name': 'e', 'descriptor': 'J', 'modifiers': 18}
    return fields


class FileView:
    """Read-only virtual paths let public records use the same byte validator."""
    def __init__(self, files, name=''):
        self.files, self.name = files, name
    def __truediv__(self, value):
        return FileView(self.files, self.name+'/'+str(value) if self.name else str(value))
    def read_bytes(self):
        require(self.name in self.files, 'missing captured file: '+self.name)
        return self.files[self.name]
    def exists(self):
        return self.name in self.files


def source_trees():
    wrong = rooted(replace(FULL, 'RandomSeed', integer('RandomSeed', 23)))
    codecs = {'defaults': rooted(compound('Data', [])), 'full': rooted(),
              'bool-zero': rooted(replace(replace(FULL, 'raining', tag(1, 'raining', 0)), 'thundering', tag(1, 'thundering', 0))),
              'wrong-seed': wrong, 'wrong-int': rooted(replace(FULL, 'SpawnY', long('SpawnY', 3))),
              'wrong-name': rooted(replace(FULL, 'LevelName', integer('LevelName', 2))),
              'wrong-bool': rooted(replace(FULL, 'raining', integer('raining', 1))),
              'wrong-player': rooted(replace(FULL, 'Player', integer('Player', 4))),
              'wrong-dimension': rooted(replace(FULL, 'Player', compound('Player', [long('Dimension', -1)]))),
              'missing-data': compound('', [string('UnknownRoot', 'no Data')])}
    return codecs, rooted(replace(FULL, 'RandomSeed', long('RandomSeed', 88)))


def fixture_files():
    codecs, backup = source_trees()
    full, wrong = codecs['full'], codecs['wrong-seed']
    layouts = {'primary': {'level.dat': full, 'level.dat_old': backup},
               'primary-absent': {'level.dat_old': backup},
               'primary-wrong-type': {'level.dat': wrong, 'level.dat_old': backup},
               'primary-corrupt': {'level.dat': b'not gzip', 'level.dat_old': backup},
               'missing-data': {'level.dat': codecs['missing-data'], 'level.dat_old': backup},
               'wrong-data': {'level.dat': compound('', [integer('Data', 7)]), 'level.dat_old': backup},
               'both-bad': {'level.dat': wrong, 'level.dat_old': b'bad backup'},
               'new-only': {'level.dat_new': full}}
    out = {'codec-'+name+'/input.dat': gzip(encode(tree)) for name, tree in codecs.items()}
    for name, files in layouts.items():
        out.update({'handler-'+name+'/world/'+filename: raw if type(raw) is bytes else gzip(encode(raw))
                    for filename, raw in files.items()})
    out['handler-old-lock/world/session.lock'] = bytes.fromhex('ffffffffffffffff')+b'prior trailing'
    out['.level-reference-owned'] = MARKER
    return out


def validate_inputs(before):
    expected = fixture_files()
    require(set(before) == set(expected), 'complete fixed metadata input inventory')
    for name, raw in expected.items():
        require(type(before[name]) is bytes and len(before[name]) <= MAX_FILE, 'bounded captured input')
        if raw.startswith(b'\x1f\x8b'):
            wanted = parse_file(FileView({name: raw}, name))
            actual = parse_file(FileView(before, name))
            require(same_json(order_only(actual), order_only(wanted)), 'fixed metadata fixture tree: '+name)
        else:
            require(before[name] == raw, 'fixed malformed/lock/marker bytes: '+name)


def serialized(info, player, saved_at):
    values = [long('RandomSeed', signed(info['random_seed_hex'])),
              *[integer(name, value) for name, value in zip(('SpawnX', 'SpawnY', 'SpawnZ'), info['spawn'])],
              long('Time', signed(info['time_hex'])), long('LastPlayed', saved_at),
              long('SizeOnDisk', signed(info['size_on_disk_hex'])),
              tag(8, 'LevelName', info['level_name_utf16']), integer('version', info['version']),
              integer('rainTime', info['rain_time']), tag(1, 'raining', int(info['raining'])),
              integer('thunderTime', info['thunder_time']), tag(1, 'thundering', int(info['thundering']))]
    if player is not None:
        # This models only the already verified original NBT writer rules on
        # input Player data. Original/C comparisons use order_only, not this.
        values.append(normalized(player))
    return compound('', [compound('Data', values)])


def validate_original(result, side, before, after, jar_url):
    require(side in PIN, 'selected original side')
    validate_inputs(before)
    keys(result, ('schema', 'side', 'jar_sha256', 'platform_parent', 'class_origins', 'bindings', 'fields', 'codecs', 'handlers'), 'original record')
    require(type(result['schema']) is int and result['schema'] == 1 and result['side'] == side and
            result['jar_sha256'] == PIN[side] and result['platform_parent'] is True, 'exact original metadata context')
    origins = {name[:-6]: {'origin': jar_url, 'loader': 'isolated'} for name in PINNED_CLASSES[side]}
    require(same_json(result['class_origins'], origins) and same_json(result['bindings'], binding_templates(side)) and
            same_json(result['fields'], field_templates(side)), 'complete exact original class/method/field bindings')
    require(type(result['codecs']) is list and all(type(row) is dict for row in result['codecs']) and
            [x.get('case') for x in result['codecs']] == list(CODECS) and
            type(result['handlers']) is list and all(type(row) is dict for row in result['handlers']) and
            [x.get('case') for x in result['handlers']] == list(HANDLERS), 'complete ordered metadata corpus')
    codecs, backup = source_trees()
    # Fixture source trees are fixed even if a caller forges both copied input
    # hashes. Compression byte differences across zlib releases are harmless.
    for name, tree in codecs.items():
        actual = parse_file(FileView(before)/('codec-'+name)/'input.dat')
        require(same_json(order_only(actual), order_only(tree)), 'fixed original input tree: '+name)
    for row in result['codecs']:
        valid = row['case'] in ('defaults', 'full', 'bool-zero', 'missing-data')
        keys(row, ('case', 'constructor', 'retained', 'empty-list') if valid else ('case', 'constructor'), 'codec record')
        if not valid:
            require(same_json(row['constructor'], {'status': 'exception', 'class': 'java.lang.ClassCastException'}), 'exact original type rejection')
            continue
        keys(row['constructor'], ('status', 'info'), 'metadata constructor')
        require(row['constructor']['status'] == 'ok', 'successful original constructor')
        info_schema(row['constructor']['info'])
        source = child(codecs[row['case']], 'Data') or compound('Data', [])
        require(same_json(row['constructor']['info'], expected_info(source)), 'all decoded original fields')
        for mode in ('retained', 'empty-list'):
            event = row[mode]
            clock_schema(event, ('status', 'info_after', 'player_identity_retained'))
            require(event['status'] == 'ok' and event['player_identity_retained'] is True and
                    same_json(event['info_after'], row['constructor']['info']), 'serializer preserves loaded state and Player identity')
            tree = parse_file(FileView(after)/('codec-'+row['case'])/(mode+'.dat'))
            last = child(child(tree, 'Data'), 'LastPlayed')['value']
            clock(event, last)
            expected = serialized(row['constructor']['info'], child(source, 'Player') if mode == 'retained' else None, last)
            require(same_json(order_only(tree), order_only(expected)), 'every original serialized metadata value and tag type')
    for row in result['handlers']:
        name = row['case']
        additions = ('save-retained', 'save-list', 'reload') if name == 'primary' else ('second_constructor', 'first_after_second', 'second_lock') if name == 'conflict' else ('changed_lock',) if name in ('missing-lock', 'short-lock', 'trailing-lock') else ()
        keys(row, ('case', 'constructor', 'initial_lock', 'load', *additions), 'handler record')
        clock_schema(row['constructor'], ('token_hex', 'players_directory', 'data_directory'))
        hex_value(row['constructor']['token_hex'], 8, 'handler token')
        for key in ('players_directory', 'data_directory'):
            require(type(row['constructor'][key]) is bool, 'typed handler directory presence')
        keys(row['initial_lock'], ('status', 'handler_token_hex', 'file_hex'), 'initial lock')
        if name == 'primary':
            for mode in ('save-retained', 'save-list'):
                event = row[mode]
                clock_schema(event, ('status', 'info_before', 'info_after'))
                info_schema(event['info_before'])
                info_schema(event['info_after'])
                require(event['status'] == 'returned', 'actual original handler save returned')
                expected_info_after = expected_info(FULL)
                if mode == 'save-list':
                    expected_info_after['version'] = 19132
                require(same_json(event['info_after'], expected_info_after) and same_json(event['info_before'], expected_info(FULL)), 'handler save state and list-only version')
                tree = parse_file(FileView(after)/'handler-primary'/(mode+'.dat'))
                last = child(child(tree, 'Data'), 'LastPlayed')['value']
                expected = serialized(expected_info_after, child(FULL, 'Player') if mode == 'save-retained' else None, last)
                require(same_json(order_only(tree), order_only(expected)), 'complete original handler serialization')
            info_schema(row['reload'])
        require(type(row['load']) is dict, 'typed handler load')
        if row['load'].get('status') == 'info':
            keys(row['load'], ('status', 'info'), 'handler load')
            info_schema(row['load']['info'])
        else:
            require(row['load'] == {'status': 'null'}, 'actual null handler load')
        if name == 'conflict':
            clock_schema(row['second_constructor'], ('token_hex',))
            hex_value(row['second_constructor']['token_hex'], 8, 'second handler token')
            for event_name in ('first_after_second', 'second_lock'):
                event = row[event_name]
                keys(event, ('status', 'handler_token_hex', 'file_hex', *(() if event_name == 'second_lock' else ('class',))), 'conflict lock')
            require(row['first_after_second']['file_hex'] == row['second_lock']['file_hex'] == row['second_constructor']['token_hex'] and
                    row['first_after_second']['handler_token_hex'] == row['constructor']['token_hex'] and
                    row['second_lock']['handler_token_hex'] == row['second_constructor']['token_hex'], 'both genuine handler tokens and actual replacement lock')
        elif name in ('missing-lock', 'short-lock', 'trailing-lock'):
            event = row['changed_lock']
            keys(event, ('status', 'handler_token_hex', 'file_hex', *(() if name == 'trailing-lock' else ('class',))), 'changed lock')
            require(event['handler_token_hex'] == row['constructor']['token_hex'], 'retained checked handler token')
            expected_file = None if name == 'missing-lock' else event['handler_token_hex'][:14] if name == 'short-lock' else event['handler_token_hex']+'0102030405'
            require(event['file_hex'] == expected_file, 'actual deliberate lock-byte change')
        world = 'handler-'+name+'/world/session.lock'
        final_event = row.get('second_lock', row.get('changed_lock', row['initial_lock']))
        require((None if world not in after else after[world].hex()) == final_event['file_hex'], 'final lock record agrees with retained file bytes')
    counts = _validate_behavior(FileView(after), result, codecs, backup)
    # No files are silently omitted from controls or public exports.
    output_paths = {f'codec-{name}/{mode}.dat' for name in ('defaults', 'full', 'bool-zero', 'missing-data') for mode in ('retained', 'empty-list')}
    output_paths |= {'handler-primary/save-retained.dat', 'handler-primary/save-list.dat', 'observations.json'}
    locks = {'handler-'+name+'/world/session.lock' for name in HANDLERS if name != 'missing-lock'}
    require(set(after) == set(before) | output_paths | locks, 'complete exact original output file inventory')
    require(same_json(strict_json(after['observations.json']), result), 'retained complete observation bytes')
    for name, raw in before.items():
        if name not in ('handler-primary/world/level.dat', 'handler-primary/world/level.dat_old', 'handler-old-lock/world/session.lock'):
            require(after.get(name) == raw, 'original input byte preservation: '+name)
    return counts


def _validate_behavior(work,result,codecs,backup):
    expected_fail=set(CODECS)-{'defaults','full','bool-zero','missing-data'};observations={'codec_cases':0,'handler_cases':0,'metadata_outputs':0,'lock_checks':0}
    for row in result['codecs']:
        name=row['case'];observations['codec_cases']+=1
        if name in expected_fail:require(row['constructor']=={'status':'exception','class':'java.lang.ClassCastException'},'wrong exact NBT type original constructor failure');continue
        source=child(codecs[name],'Data') or compound('Data',[]);info=expected_info(source);require(row['constructor']=={'status':'ok','info':info},'all original metadata fields/defaults exact')
        for mode in ('retained','empty-list'):
            event=row[mode];require(event['info_after']==info and event['player_identity_retained'] is True,'serializer retains loaded LastPlayed/version/Player object')
            output=parse_file(work/('codec-'+name)/(mode+'.dat'));payload=child(output,'Data');actual=children(payload);require(set(actual)==set(map(u16,['RandomSeed','SpawnX','SpawnY','SpawnZ','Time','LastPlayed','SizeOnDisk','LevelName','version','rainTime','raining','thunderTime','thundering']+(['Player'] if mode=='retained' and child(source,'Player') else []))),'original info serializer known-key projection and Player policy')
            clock(event,child(payload,'LastPlayed')['value']);require(child(payload,'version')['value']==info['version'],'pure info list serialization does not force version')
            for name_,type_ in [('RandomSeed',4),('Time',4),('LastPlayed',4),('SizeOnDisk',4),('SpawnX',3),('SpawnY',3),('SpawnZ',3),('version',3),('rainTime',3),('thunderTime',3),('LevelName',8),('raining',1),('thundering',1)]:require(child(payload,name_)['type']==type_,'exact serialized metadata tag type')
            for flag in ('raining','thundering'):require(child(payload,flag)['value']==int(info[flag]),'nonzero Boolean Byte canonicalization')
            if mode=='retained' and child(source,'Player'):require(normalized(child(payload,'Player'))==normalized(child(source,'Player')),'complete opaque Player including UTF16/NaN/empty-list writer rules')
            require(child(payload,'LevelName')['value']==info['level_name_utf16'],'exact UTF16 code units including NUL/unpaired/pair');observations['metadata_outputs']+=1
    for row in result['handlers']:
        name=row['case'];observations['handler_cases']+=1;world=work/('handler-'+name)/'world';clock(row['constructor'],signed(row['constructor']['token_hex']));require(row['constructor']['data_directory'] is True and row['constructor']['players_directory'] is (name=='true-flag'),'constructor false is not read-only; players flag only')
        initial=row['initial_lock'];require(initial['status']=='ok' and initial['file_hex']==row['constructor']['token_hex']==initial['handler_token_hex'],'eight-byte big-endian newly overwritten token');observations['lock_checks']+=1
        expected_source=FULL if name=='primary' else child(backup,'Data') if name in ('primary-absent','primary-wrong-type','primary-corrupt','wrong-data') else compound('Data',[]) if name=='missing-data' else None
        require(row['load']==({'status':'null'} if expected_source is None else {'status':'info','info':expected_info(expected_source)}),'primary/old/default/no-new exact selection')
        if name=='primary':
            first=parse_file(work/'handler-primary/save-retained.dat');second=parse_file(work/'handler-primary/save-list.dat');one=child(first,'Data');two=child(second,'Data');require(child(one,'version')['value']==1234 and child(one,'Player') is not None and child(two,'version')['value']==19132 and child(two,'Player') is None,'actual handler only list-save forces version/drops retained Player')
            require(child(first,'UnknownRoot') is None and child(one,'UnknownData') is None,'original fresh root/Data discard unknown fields')
            for mode,payload in [('save-retained',one),('save-list',two)]:clock(row[mode],child(payload,'LastPlayed')['value']);require(row[mode]['info_after']['last_played_hex']==expected_info(FULL)['last_played_hex'],'handler save does not update loaded f')
            require((world/'level.dat_old').read_bytes()==(work/'handler-primary/save-retained.dat').read_bytes() and (world/'level.dat').read_bytes()==(work/'handler-primary/save-list.dat').read_bytes() and not (world/'level.dat_new').exists(),'two real saves rotate old/current and remove new');require(row['reload']==expected_info(two),'genuine final handler reload');observations['metadata_outputs']+=2
        if name=='conflict':
            second=row['second_constructor'];clock(second,signed(second['token_hex']));require(second['token_hex']!=row['constructor']['token_hex'],'real distinct millisecond handler tokens observed');require(row['first_after_second']['status']=='exception' and row['first_after_second']['class']==('us' if result['side']=='client' else 'my') and row['second_lock']['status']=='ok','second constructor invalidates prior token check');observations['lock_checks']+=2
        elif name in ('missing-lock','short-lock','trailing-lock'):
            event=row['changed_lock'];require(event['status']==('ok' if name=='trailing-lock' else 'exception'),'actual missing/short/trailing lock behavior')
            if name!='trailing-lock':require(event['class']==('us' if result['side']=='client' else 'my'),'exact original lock exception class')
            if name=='trailing-lock':require(event['file_hex']==row['constructor']['token_hex']+'0102030405','trailing bytes ignored and retained')
            observations['lock_checks']+=1
    require(observations=={'codec_cases':10,'handler_cases':16,'metadata_outputs':10,'lock_checks':21},'complete finite case counts');return observations


PINNED_CODE = {'client': {'compound_ctor': {'bytes': 16, 'sha256': '425b169627fb4c4110a332b3f13115a079b37386c676508ef638ba2a01b5e9ab'}, 'info_ctor': {'bytes': 167, 'sha256': 'e444dbe3144ebd0c414cff262ee40dc939e0c10e9ef828a23e4755a357d8f216'}, 'handler_ctor': {'bytes': 8, 'sha256': '5577af645a3cd2252842cf0a29d3b3fe6abfe2a342178b2a3c72b79e25f7f689'}, 'get_compound': {'bytes': 35, 'sha256': '245af2d00d29370bf057f023736f3e16cb274e13c3482baf0f9e1d14a23d749c'}, 'put_compound': {'bytes': 17, 'sha256': '2f4b6294d755e38152b4d1e82cb51f6f35298f2a5e867514fd13b2f1c8d7cb98'}, 'read_gzip': {'bytes': 34, 'sha256': '9ceb177ca3a5f9d75194e49ec405e6d703ca33483cd1bfeb71ce69488fd44952'}, 'write_gzip': {'bytes': 36, 'sha256': '21555248050edd220519c7ed8eaba3a693b29096c9df2fca76f5d4593a0ac253'}, 'info_retained': {'bytes': 19, 'sha256': 'd8e5e5b6c64dc9add38adee31ab8fd8e8f7a6076cb699d3e450cca3d83b80285'}, 'info_list': {'bytes': 61, 'sha256': '9bc4ab23eae7f7395ddd0ee3e8b0464ae0d535c089766c47401059f71079df88'}, 'handler_load': {'bytes': 110, 'sha256': 'aa1ee25b3c769b86395b837525e6fcd99ed4358c3e92b08709881ef14c47deb8'}, 'handler_save_retained': {'bytes': 147, 'sha256': '2f53a62617d02a334d260239b0ec3a7ea5b27015a323d8680033b52e78926beb'}, 'handler_save_list': {'bytes': 14, 'sha256': '1b240f45898ef6dab8789a3106214ad060ecaf938eec698317e8ecfe80b20af6'}, 'handler_check_lock': {'bytes': 81, 'sha256': 'bb68a2319df7fd1b9304355ee68a495810957cf46bd7554ef6a760577b8f8260'}}, 'server': {'compound_ctor': {'bytes': 16, 'sha256': '5af0c53ab1b39f2e3fa057af948fc489dcaf182927e29c62410597856d7b90ea'}, 'info_ctor': {'bytes': 167, 'sha256': '48b8850df309ac443ca185562ac81aa97a6f6a9ea5dc71acb230c660263d7fdd'}, 'handler_ctor': {'bytes': 8, 'sha256': '0a89ec8d2eb1c824548206cf5622efd57da63ee5432ca47400b7266a042fe111'}, 'get_compound': {'bytes': 35, 'sha256': 'faf93050df89070aed6459edefe3e27bb28863ed00a6ee11586c038dd376fa78'}, 'put_compound': {'bytes': 17, 'sha256': 'bdc4e36ee9caa9d8d187048b658dbea5576691ef481b1f18630532ffa91368b3'}, 'read_gzip': {'bytes': 34, 'sha256': '03678be316a9a4fa024e3e1f3f495433001f4abaa67b8c67e4097e1fedcf1096'}, 'write_gzip': {'bytes': 36, 'sha256': '4ea62893ef53bfa4eaeb035616fce99bbe27e17d55602d12414b26782261c84f'}, 'info_retained': {'bytes': 19, 'sha256': '22eed1b0a2000d6097c6e2da1d6063da855c7b35c236cd666769c52c40d29536'}, 'info_list': {'bytes': 61, 'sha256': '5f9dd859f731e7a165e14e3f99998f6c45897af1053b3905f08cdf94964eb54f'}, 'handler_load': {'bytes': 110, 'sha256': '41d927b8cef30c7b1a70bfbacd3bc591ecb06133cc6606ab8a7d474e58f20d4f'}, 'handler_save_retained': {'bytes': 147, 'sha256': 'c8b71ce337a573740557d34d432bd7b6fb7ff32691cadcf707c001ba840dc2c7'}, 'handler_save_list': {'bytes': 14, 'sha256': '7b62d683f628803f4ab5d5e02e4a6a00eabac19b1e0a1e04405680e9e5aab246'}, 'handler_check_lock': {'bytes': 81, 'sha256': '5479dbd16e682f5e2f4606191e04c945bb9a4a32807e2b294532d2d0bad04bf7'}}}
PINNED_CLASSES = {'client': {'nu.class': {'bytes': 4122, 'sha256': 'a1a5701ec2180d01ddc604cc8c2e5aff1176b10faf83bdb64d2f1cc8c806ac4a'}, 'ei.class': {'bytes': 3748, 'sha256': '5d01d1dda7be346c57fd00846d2425615186c3ef1c65c06aa0bd0f45aa9f7fef'}, 'mx.class': {'bytes': 617, 'sha256': 'a0b3d564fd55705c52fc2fca350afcad516b224d03ca21ea9485908764f85417'}, 'fm.class': {'bytes': 3482, 'sha256': '82f848599887eaf5cd033d6886db167a3abdf1680151ac9e61fd4ca71a22b529'}, 'as.class': {'bytes': 1077, 'sha256': 'c840daad77df4a1ad92367abce5b3d2035362bc2b010df1ff7b4faee3bbfb652'}}, 'server': {'iq.class': {'bytes': 4118, 'sha256': '9e669793b602888407b6d092eaaf5778b0edaa79f4692da8a995efc63ded2223'}, 'ct.class': {'bytes': 3400, 'sha256': 'ce28db56968ac4129348e09d956e7f33fa809db04130cbdbb2856b89dd44b68f'}, 'ie.class': {'bytes': 687, 'sha256': '2dcd09358ba2f3878d282848eb2fa4056b6b175ac6d730d195e62b27f89b306b'}, 'dq.class': {'bytes': 4210, 'sha256': '5e438f929d21aa27c5d1d357b7fd9075c45ba49242934ef350acaa442a602278'}, 'ag.class': {'bytes': 1077, 'sha256': 'd0eb17e39056968710c661d07a1880263a47c67fab9c1779975097155bd4d4c8'}}}



def helpers():
    require(Path(__file__).resolve() == (ROOT/'scripts/reference/storage/compare_level.py').resolve(), 'canonical executing source identity')
    path = ROOT/'scripts/reference/chunk/compare_chunk.py'
    raw = path.read_bytes()
    module = types.ModuleType('_level_process_entry')
    module.__file__ = str(path)
    exec(compile(raw, str(path), 'exec'), module.__dict__)
    common, executed = module.shared_helper(ROOT)
    executed.append({'path': str(path.resolve()), **fingerprint(raw)})
    return common, executed


def original_bindings(result, side, jar):
    require(file_record(jar)['sha256'] == PIN[side], 'exact selected original archive bytes')
    bodies = {}
    classes = {}
    with zipfile.ZipFile(jar) as archive:
        parsed = {}
        for name, expected in PINNED_CLASSES[side].items():
            raw = archive.read(name)
            require(fingerprint(raw) == expected, 'pinned original class bytes')
            classes[name] = fingerprint(raw)
            parsed[name[:-6]] = class_members(raw)
        for role, row in binding_templates(side).items():
            actual = parsed[row['class']][1][(row['method'], row['signature'])]
            require(actual['modifiers'] == row['modifiers'] and actual['code'] is not None and
                    fingerprint(actual['code']) == PINNED_CODE[side][role], 'pinned actual original method body')
            bodies[role] = {**row, **fingerprint(actual['code'])}
        for row in field_templates(side).values():
            require(parsed[row['class']][0][(row['name'], row['descriptor'])] == row['modifiers'], 'pinned original field descriptor and modifiers')
    require(same_json(result['bindings'], binding_templates(side)), 'recorded exact method bindings')
    return {'classes': classes, 'methods': bodies, 'fields': field_templates(side)}


def read_file_map(directory, common=None):
    if common is not None:
        common.no_link_components(directory)
        actual = common.world_inventory(directory, hashes=True)
        require(actual['complete'] and actual['error'] is None and actual['files'] <= 128 and actual['bytes'] <= 8*1024*1024,
                'bounded complete nonlinked file traversal')
        result = {}
        for row in actual['entries']:
            if row['kind'] == 'file':
                require(row['bytes'] <= MAX_RECORD, 'bounded retained file')
                path = directory/row['path']
                common.no_link_components(path)
                with path.open('rb') as stream:
                    raw = stream.read(MAX_RECORD+1)
                require(fingerprint(raw) == {key: row[key] for key in ('bytes', 'sha256')}, 'retained file stable while reading')
                result[row['path']] = raw
        return result
    result = {}
    for path in sorted(directory.rglob('*')):
        require(not path.is_symlink(), 'no linked retained files')
        if path.is_file():
            if common is not None:
                common.no_link_components(path)
            require(len(result) < 128 and path.stat().st_size <= MAX_RECORD, 'bounded complete retained files')
            result[path.relative_to(directory).as_posix()] = path.read_bytes()
    require(sum(map(len, result.values())) <= 8*1024*1024, 'retained file byte budget')
    return result


def file_map_fingerprints(files):
    return {name: fingerprint(raw) for name, raw in sorted(files.items())}


def archive_files(files, destination):
    destination.mkdir()
    for name, raw in files.items():
        require(name and not name.startswith('/') and '\\' not in name and all(x not in ('', '.', '..') for x in name.split('/')), 'owned relative archive path')
        path = destination/name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(raw)


def directory_names(after=False):
    result = {'codec-'+name for name in CODECS}
    result |= {'handler-'+name for name in HANDLERS}
    result |= {'handler-'+name+'/world' for name in HANDLERS}
    if after:
        result |= {'handler-'+name+'/world/data' for name in HANDLERS}
        result.add('handler-true-flag/world/players')
    return result


def validate_inventory(inventory, files, after=False):
    keys(inventory, ('root', 'exists', 'complete', 'error', 'files', 'directories', 'bytes', 'entries'), 'owned inventory')
    require(type(inventory['root']) is str and 1 <= len(inventory['root']) <= 4096 and
            inventory['complete'] is True and inventory['error'] is None and
            inventory['exists'] is True and type(inventory['entries']) is list, 'complete actual owned inventory')
    entries = inventory['entries']
    require(all(type(row) is dict and type(row.get('path')) is str for row in entries), 'typed inventory entries')
    require(len(entries) <= 256 and len({row.get('path') for row in entries}) == len(entries), 'bounded unique inventory paths')
    expected = [{'path': name, 'kind': 'file', **record} for name, record in file_map_fingerprints(files).items()]
    expected += [{'path': name, 'kind': 'directory'} for name in directory_names(after)]
    expected.sort(key=lambda row: row['path'])
    require(same_json(entries, expected) and type(inventory['files']) is int and inventory['files'] == len(files) and
            type(inventory['directories']) is int and inventory['directories'] == len(directory_names(after)) and
            type(inventory['bytes']) is int and inventory['bytes'] == sum(map(len, files.values())), 'full owned directory and file-byte inventory')


def control_signature(files):
    result = {}
    sampled = {f'codec-{name}/{mode}.dat' for name in ('defaults', 'full', 'bool-zero', 'missing-data') for mode in ('retained', 'empty-list')}
    sampled |= {'handler-primary/save-retained.dat', 'handler-primary/save-list.dat', 'handler-primary/world/level.dat', 'handler-primary/world/level.dat_old'}
    for name, raw in sorted(files.items()):
        if not name.endswith('.dat'):
            continue
        if raw in (b'not gzip', b'bad backup'):
            result[name] = {'raw_hex': raw.hex()}
            continue
        tree = parse_file(FileView(files, name))
        if name in sampled:
            child(child(tree, 'Data'), 'LastPlayed')['value'] = 'independently-validated-clock'
        result[name] = order_only(tree)
    return result


def validate_controls(runs):
    require(type(runs) is list and [(row['side'], row['repeat']) for row in runs] ==
            [('client', 0), ('client', 1), ('server', 0), ('server', 1)], 'all four ordered original/control contexts')
    signatures = [control_signature(row['after_files']) for row in runs]
    require(all(same_json(signatures[0], value) for value in signatures[1:]), 'all complete paired/control typed trees; only sampled LastPlayed and compound order differ')


def c_projection(info):
    return {'ok': True, 'seed': info['random_seed_hex'], 'spawn': info['spawn'], 'time': info['time_hex'],
            'last_played': info['last_played_hex'], 'size_on_disk': info['size_on_disk_hex'],
            'level_name': info['level_name_utf16'], 'version': info['version'], 'rain_time': info['rain_time'],
            'raining': info['raining'], 'thunder_time': info['thunder_time'], 'thundering': info['thundering'],
            'has_player': info['player_present'], 'player_dimension': info['dimension']}


def replay_plan(result, files):
    plans = []
    wrong = {'wrong-seed': 'RandomSeed', 'wrong-int': 'SpawnY', 'wrong-name': 'LevelName', 'wrong-bool': 'raining', 'wrong-player': 'Player', 'wrong-dimension': 'Dimension'}
    for row in result['codecs']:
        name = row['case']
        raw = encode(parse_file(FileView(files)/('codec-'+name)/'input.dat'))
        expected = {'ok': False, 'status': 'wrong level field type', 'field': wrong[name]} if name in wrong else c_projection(row['constructor']['info'])
        plans.append({'label': name+'-decode', 'argv': ['decode'], 'input': raw, 'expected': expected, 'kind': 'decode'})
        if name in wrong:
            continue
        for mode, player in (('retained', 'keep'), ('empty-list', 'drop')):
            tree = parse_file(FileView(files)/('codec-'+name)/(mode+'.dat'))
            clock_ = child(child(tree, 'Data'), 'LastPlayed')['value']
            plans.append({'label': name+'-'+mode, 'argv': ['build', f'{clock_ & ((1 << 64)-1):016x}', player, 'fresh'], 'input': raw, 'expected': tree, 'kind': 'original-fresh-serializer'})
    # This checks the saved schema, not implementation of handler IO or locks.
    for mode in ('save-retained', 'save-list'):
        tree = parse_file(FileView(files)/'handler-primary'/(mode+'.dat'))
        clock_ = child(child(tree, 'Data'), 'LastPlayed')['value']
        plans.append({'label': 'handler-saved-schema-'+mode, 'argv': ['build', f'{clock_ & ((1 << 64)-1):016x}', 'keep', 'fresh'], 'input': encode(tree), 'expected': tree, 'kind': 'original-handler-saved-schema-only'})
    require(len(plans) == 20, 'complete 20-call pure codec replay')
    return plans


def compare_c(value, plan):
    if plan['kind'] == 'decode':
        require(same_json(value, plan['expected']), 'strict complete C decoded fields/type diagnostics: '+plan['label'])
    else:
        keys(value, ('ok', 'nbt_hex'), 'C serialization response')
        require(value['ok'] is True and type(value['nbt_hex']) is str and len(value['nbt_hex']) <= MAX_FILE*2 and
                len(value['nbt_hex']) % 2 == 0 and re.fullmatch(r'[0-9a-f]*', value['nbt_hex']) is not None, 'bounded canonical C raw NBT hex')
        actual = parse_raw(bytes.fromhex(value['nbt_hex']))
        require(same_json(order_only(actual), order_only(plan['expected'])), 'all original/C output tag types, values, UTF16, list order and payload bits; compound order only: '+plan['label'])


PROCESS_FIELDS = ('command', 'returncode', 'seconds', 'timed_out', 'stop_reason',
                  'stdout_observed', 'stderr_observed', 'stdout_retained', 'stderr_retained', 'inventory_polls')
OBSERVED_PROCESS_FIELDS = ('clock_before', 'clock_after', 'stdout', 'stderr')


def successful(process, extra=()):
    keys(process, (*PROCESS_FIELDS, *extra), 'bounded process')
    require(type(process['returncode']) is int and process['returncode'] == 0 and
            process.get('timed_out') is False and process.get('stop_reason') is None and
            process.get('stdout_observed') == process.get('stdout_retained') and
            process.get('stderr_observed') == process.get('stderr_retained'), 'successful bounded complete process')
    for name in ('stdout_observed', 'stderr_observed', 'stdout_retained', 'stderr_retained', 'inventory_polls'):
        integer_value(process.get(name), 0, 2*1024*1024, 'complete process counters')
    require(type(process.get('command')) is list and 1 <= len(process['command']) <= 64 and
            all(type(item) is str and 1 <= len(item) <= 4096 for item in process['command']) and
            type(process.get('seconds')) in (int, float) and 0 <= process['seconds'] <= 120, 'bounded actual process command and duration')


def observed_process(process, *, paths=False):
    successful(process, OBSERVED_PROCESS_FIELDS)
    for name in ('clock_before', 'clock_after'):
        integer_value(process[name], -(1 << 63), (1 << 63)-1, 'measured process wall clock')
    for name in ('stdout', 'stderr'):
        record = fingerprint_record(process[name], path=paths)
        require(record['bytes'] == process[name+'_observed'], 'complete retained process stream byte count')


def checked_process(common, command, directory, name, *, world=None, stdin=None, seconds=90):
    start = int(time.time()*1000)
    out, err = directory/(name+'-out.txt'), directory/(name+'-err.txt')
    result = common.run_live([str(x) for x in command], out, err, directory, seconds=seconds,
                             stdout_limit=MAX_RECORD, stderr_limit=MAX_RECORD, world_root=world, stdin_path=stdin)
    record = {**asdict(result), 'clock_before': start, 'clock_after': int(time.time()*1000),
              'stdout': common.file_record(out, byte_limit=MAX_RECORD), 'stderr': common.file_record(err, byte_limit=MAX_RECORD)}
    observed_process(record, paths=True)
    return record


def validate_clock_window(value, process):
    for name in ('clock_before', 'clock_after'):
        integer_value(process[name], -(1 << 63), (1 << 63)-1, 'measured child wall clock')
    low, high = sorted((process['clock_before'], process['clock_after']))
    def walk(node):
        if type(node) is dict:
            if 'clock_before' in node:
                require(low-1000 <= node['clock_before'] <= high+1000 and low-1000 <= node['clock_after'] <= high+1000, 'original clock event lies in measured child wall-clock interval')
            for item in node.values():
                walk(item)
        elif type(node) is list:
            for item in node:
                walk(item)
    walk(value)


def validate_work_dir(work_dir, protected=()):
    """Refuse redirected/public output trees before creating any artifacts."""
    work = Path(os.path.abspath(work_dir))
    for component in reversed((work, *work.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(info.st_mode) and not (getattr(info, 'st_file_attributes', 0) & 0x400),
                'output link/reparse component refused')
        require(stat.S_ISDIR(info.st_mode), 'output must have ordinary directory components')
    resolved = work.resolve()
    if resolved.is_relative_to(ROOT):
        parts = resolved.relative_to(ROOT).parts
        require(parts and (parts[0] in ('.local', 'build') or parts[0].startswith('build-')),
                'output inside public source/input layout refused')
    for selected in protected:
        if selected is None:
            continue
        path = Path(selected).resolve()
        require(resolved != path and resolved not in path.parents,
                'output would contain/alias an attributed input')
        if work.exists() and path.exists():
            require(not os.path.samefile(work, path), 'output aliases an input')
    return work


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('client', 'server', 'work-dir'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--capture-only', action='store_true')
    parser.add_argument('--tested-commit', default='')
    for name in ('java', 'javac', 'javap'):
        parser.add_argument('--'+name)
    args = parser.parse_args(argv)
    output, common, inputs = None, None, {}
    report = {'schema': 1, 'status': 'running', 'tested_commit': args.tested_commit or None, 'inputs_before': inputs, 'errors': []}
    status, executing = 1, False
    try:
        common, executed = helpers()
        require(not args.tested_commit or re.fullmatch(r'[0-9a-f]{40}', args.tested_commit), 'complete lowercase tested source label')
        require(not any(os.environ.get(name) for name in JAVA_ENVIRONMENT), 'uninjected Java environment')
        require(args.capture_only or args.executable is not None, '--executable required for live C comparison')
        java_source = ROOT/'tests/reference/storage/LevelObservation.java'
        attributed = [Path(__file__).resolve(), java_source, Path(sys.executable).resolve(), ROOT/'CMakeLists.txt', ROOT/'tests/storage/level/level_probe.c']
        attributed += list((ROOT/'src').rglob('*.c')) + list((ROOT/'src').rglob('*.h')) + list((ROOT/'include').rglob('*.h'))
        attributed += [Path(row['path']) for row in executed]
        work = validate_work_dir(args.work_dir, attributed+[args.client, args.server, args.executable, args.java, args.javac, args.javap])
        tools = common.runtime_tools(args)
        release = tools['jdk_release'].read_text(encoding='utf-8')
        match = re.search(r'(?m)^JAVA_VERSION="([0-9]+)(?:\.[^"]*)?"', release)
        if match is None or int(match.group(1)) < 17:
            raise common.Unavailable('metadata helper requires JDK17 or newer')
        jars = {side: common.no_link_components(getattr(args, side)).resolve() for side in PIN}
        for side, jar in jars.items():
            common.no_link_components(jar)
            if not jar.is_file():
                raise common.Unavailable('missing supplied '+side+' original archive')
            require(common.file_record(jar, byte_limit=2*1024*1024)['sha256'] == PIN[side], 'pinned original archive hash')
        if not args.capture_only and not args.executable.is_file():
            raise common.Unavailable('missing C metadata executable')
        if args.executable is not None:
            common.no_link_components(args.executable)
        attributed += list(jars.values())+list(tools.values())
        if args.executable is not None:
            attributed.append(args.executable.resolve(strict=True))
        for path in attributed:
            common.no_link_components(path)
            inputs[str(path.resolve())] = common.file_record(path, byte_limit=512*1024*1024)
        for row in executed:
            require(inputs[str(Path(row['path']).resolve())]['sha256'] == row['sha256'], 'executed helper source attribution')
        work = validate_work_dir(work, attributed)
        work.mkdir(parents=True, exist_ok=True)
        candidate = work/('capture-'+uuid.uuid4().hex)
        candidate.mkdir()
        output = candidate.resolve()
        report = {'schema': 1, 'status': 'running', 'mode': 'original-only' if args.capture_only else 'live-original-C',
                  'tested_commit': args.tested_commit or None, 'source_label': 'Caller label; actual source, executable and helper bytes are attributed independently.',
                  'started_utc': common.utc_now(), 'artifact_root': str(output), 'python': sys.version,
                  'platform': platform.platform(), 'executed_source_modules': executed, 'runs': [], 'c_records': [], 'errors': [],
                  'limits': {'child_seconds': 90, 'heap_mib': 128, 'stream_bytes': MAX_RECORD, 'owned_entries': 256, 'owned_bytes': 32*1024*1024,
                             'policy': 'Bounded children and monitored complete inventories, not an OS process-tree or filesystem quota.'},
                  'scope': 'Pure metadata codec parity only. Genuine handler fallback, two-save rotation and session-lock observations are retained without a native handler equivalence claim. Unknown root/Data preservation is a separate C safety policy.'}
        report['inputs_before'] = inputs
        common.write_json(output/'manifest.json', report)
        for name in ('source', 'classes', 'empty'):
            (output/name).mkdir()
        (output/'source'/java_source.name).write_bytes(java_source.read_bytes())
        (output/'source'/'compare_level.py').write_bytes(Path(__file__).read_bytes())
        executing = True
        common.tool_versions(tools, output, report)
        require(all(row['major'] >= 17 for row in report['versions'].values()), 'actual JDK17 tools')
        compile_command = [tools['javac'], '-J-Xmx512m', '--release', '17', '-proc:none', '-implicit:none', '-encoding', 'UTF-8', '-Xlint:all', '-Werror', '-classpath', output/'empty', '-sourcepath', output/'empty', '--module-path', output/'empty', '-d', output/'classes', output/'source'/java_source.name]
        report['compile'] = checked_process(common, compile_command, output, 'compile', seconds=30)
        class_names = {'LevelObservation.class', 'LevelObservation$Probe.class', 'LevelObservation$OriginalFailure.class'}
        require({p.name for p in (output/'classes').iterdir()} == class_names, 'complete exact helper class inventory')
        report['classes_before'] = {name: common.file_record(output/'classes'/name, byte_limit=MAX_RECORD) for name in sorted(class_names)}
        for name in class_names:
            require((output/'classes'/name).read_bytes()[:8] == b'\xca\xfe\xba\xbe\x00\x00\x00\x3d', 'strict Java17 helper class')
        validated = []
        for side, jar in jars.items():
            for repeat in (0, 1):
                label = side+'-'+str(repeat)
                directory = output/label
                directory.mkdir()
                for name in directory_names():
                    (directory/name).mkdir(parents=True, exist_ok=True)
                for name, raw in fixture_files().items():
                    (directory/name).write_bytes(raw)
                before = read_file_map(directory, common)
                before_inventory = common.world_inventory(directory, hashes=True)
                validate_inventory(before_inventory, before)
                archive_files(before, output/(label+'-before'))
                process = checked_process(common, [tools['java'], '-Xint', '-Xmx128m', '-cp', output/'classes', 'LevelObservation', jar, side, directory, directory/'observations.json'], output, label, world=directory)
                after = read_file_map(directory, common)
                value = strict_json(after['observations.json'])
                counts = validate_original(value, side, before, after, jar.as_uri().replace('file:///', 'file:/', 1))
                validate_clock_window(value, process)
                after_inventory = common.world_inventory(directory, hashes=True)
                validate_inventory(after_inventory, after, True)
                archive_files(after, output/(label+'-after'))
                binding = original_bindings(value, side, jar)
                row = {'side': side, 'repeat': repeat, 'process': process, 'counts': counts,
                       'observation': common.file_record(directory/'observations.json', byte_limit=MAX_RECORD),
                       'before': before_inventory, 'after': after_inventory, 'original_bindings': binding}
                report['runs'].append(row)
                validated.append({'side': side, 'repeat': repeat, 'after_files': after})
                if not args.capture_only:
                    c_directory = output/(label+'-c')
                    c_directory.mkdir()
                    for plan in replay_plan(value, after):
                        stdin = c_directory/(plan['label']+'-in.txt')
                        stdin.write_bytes(plan['input'].hex().encode('ascii')+b'\n')
                        c_process = checked_process(common, [args.executable.resolve(), *plan['argv']], c_directory, plan['label'], stdin=stdin, seconds=15)
                        require(c_process['stderr_retained'] == 0, 'successful C codec has no stderr/sanitizer diagnostics')
                        raw_output = Path(c_process['stdout']['path']).read_bytes()
                        require(raw_output.endswith(b'\n') and raw_output.count(b'\n') == 1, 'one complete C JSON response')
                        compare_c(strict_json(raw_output), plan)
                        report['c_records'].append({'side': side, 'repeat': repeat, 'label': plan['label'], 'kind': plan['kind'], 'input': common.file_record(stdin, byte_limit=MAX_RECORD), 'process': c_process, 'comparison': 'pass'})
                common.write_json(output/'manifest.json', report)
        validate_controls(validated)
        report['comparisons'] = {'client/control': 'pass', 'server/control': 'pass', 'client/server': 'pass'}
        require(len(report['c_records']) == (0 if args.capture_only else 80), 'all pure-C replay records')
        report['status'], status = 'pass', 0
    except Exception as error:
        unavailable = common is not None and isinstance(error, common.Unavailable) and not executing
        status = 77 if unavailable else 1
        if report is not None:
            report['status'] = 'unavailable' if unavailable else 'fail'
            report['errors'].append(str(error))
        if output is not None:
            print(('UNAVAILABLE: ' if unavailable else 'FAILURE: ')+str(error), file=sys.stderr)
    finally:
        if report is not None:
            report['inputs_after'] = {}
            for name, before in inputs.items():
                try:
                    current = common.file_record(Path(name), byte_limit=before['bytes'])
                    report['inputs_after'][name] = current
                    require(current == before, 'attributed input changed: '+name)
                except Exception as error:
                    report['status'], status = 'fail', 1
                    report['errors'].append('attributed input changed: '+name+': '+str(error))
            if 'classes_before' in report:
                try:
                    current = {p.name: common.file_record(p, byte_limit=MAX_RECORD) for p in sorted((output/'classes').iterdir())}
                    report['classes_after'] = current
                    require(current == report['classes_before'], 'compiled helper inventory or bytes changed')
                except Exception as error:
                    report['status'], status = 'fail', 1
                    report['errors'].append(str(error))
            if common is not None:
                report['finished_utc'] = common.utc_now()
            if output is None:
                print(json.dumps(report), file=sys.stderr)
            else:
                try:
                    common.write_json(output/'manifest.json', report)
                    print(report['status'].upper()+': '+str(output/'manifest.json'))
                except Exception as error:
                    report['status'], status = 'fail', 1
                    report['errors'].append('cannot retain outcome manifest: '+str(error))
                    print(json.dumps(report), file=sys.stderr)
    return status


if __name__ == '__main__':
    sys.exit(main())
