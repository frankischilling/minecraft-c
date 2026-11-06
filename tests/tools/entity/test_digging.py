"""Native QM constructor/hit/destruction/tick/draw controls with authored inputs."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zlib
import struct

ROOT = Path(__file__).resolve().parents[3]
sys.dont_write_bytecode = True

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def fingerprint(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def authored_dirt_world(module,path):
    module.authored_world(path)
    marker=b'\x07\x00\x06Blocks'+struct.pack('>i',32768)
    for region in (path/'region').glob('*.mcr'):
        data=bytearray(region.read_bytes())
        for index in range(1024):
            location,=struct.unpack_from('>I',data,index*4)
            if not location:
                continue
            offset=(location>>8)*4096
            length,=struct.unpack_from('>I',data,offset)
            assert location&255==1 and data[offset+4]==2
            raw=zlib.decompress(data[offset+5:offset+4+length])
            assert raw.count(marker)==1
            start=raw.index(marker)+len(marker)
            blocks=raw[start:start+32768]
            assert len(blocks)==32768 and blocks.count(1)==256 and set(blocks)=={0,1}
            raw=raw[:start]+blocks.replace(b'\x01',b'\x03')+raw[start+32768:]
            payload=b'\x02'+zlib.compress(raw)
            record=struct.pack('>I',len(payload))+payload
            assert len(record)<=4096
            data[offset:offset+4096]=record+bytes(4096-len(record))
        region.write_bytes(data)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--work-dir',required=True,type=Path)
    args=parser.parse_args()
    args.work_dir.mkdir(parents=True,exist_ok=True)
    parent=load('digging_inputs',ROOT/'tests/tools/entity/test_item_particle.py')
    paths=[args.executable.resolve(strict=True),Path(__file__).resolve(),
        ROOT/'tests/entity/particles/test_digging.c',ROOT/'tests/entity/test_item_particle.c']
    paths += sorted(p for folder in ('include','src','tests/client/startup/fixtures')
                    for p in (ROOT/folder).rglob('*') if p.is_file())
    before={str(p):fingerprint(p) for p in paths}
    report={'status':'FAIL','scope':'Authored native component controls; no original comparison',
            'inputs_before':before}
    try:
        with tempfile.TemporaryDirectory(prefix='digging-',dir=args.work_dir) as temporary:
            root=Path(temporary)
            assets=root/'assets';assets.mkdir()
            parent.authored_assets(assets)
            png=parent.load('digging_png',ROOT/'tests/tools/client/test_startup.py').authored_png
            (assets/'terrain.png').write_bytes(png(256,256,lambda x,y:(x&255,y&255,(x^y)&255,255)))
            world=root/'world';authored_dirt_world(parent,world)
            environment=os.environ.copy();environment['ALSOFT_DRIVERS']='null'
            for name in ('APPDATA','HOME','XDG_DATA_HOME','XDG_CONFIG_HOME','XDG_CACHE_HOME'):
                environment[name]=str(root/'private-home')
            command=[str(paths[0]),str(ROOT/'tests/client/startup/fixtures'),str(assets),str(world)]
            result=subprocess.run(command,env=environment,text=True,capture_output=True,timeout=240)
            report.update(exit=result.returncode,stdout=result.stdout,stderr=result.stderr)
            print(result.stdout,end='')
            print(result.stderr,end='',file=sys.stderr)
            if result.returncode:
                raise RuntimeError('Native digging component checks failed')
            if 'genuine QM/XW/World/DN' not in result.stdout:
                raise RuntimeError('Missing actual native completion footer')
            report['status']='PASS'
    finally:
        report['inputs_after']={str(p):fingerprint(p) for p in paths}
        if report['inputs_after']!=before:
            report['status']='FAIL'
        (args.work_dir/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    if report['status']!='PASS':
        raise RuntimeError('Digging component input continuity failed')

if __name__=='__main__':
    main()
