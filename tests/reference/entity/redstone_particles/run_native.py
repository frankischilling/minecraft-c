"""Replay genuine native IM owners, exact CPU components and independent constructor entropy."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
K=module('redstone_native_packet',HERE/'packet.py')
R=module('redstone_native_resources',ROOT/'tests/reference/client/player_statistics/support.py')

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def tree(path):return {p.relative_to(path).as_posix():digest(p) for p in sorted(path.rglob('*')) if p.is_file()}
def snapshot(executable,resources):
 paths=[ROOT/'tests/entity/particles/test_redstone_reference.c',ROOT/'tests/client/statistics/test_player_dispatch.c',
        ROOT/'src/client/particles/render_item.c',ROOT/'src/client/particles/render_internal.h',
        ROOT/'include/mc/entity/particles/particle.h',ROOT/'src/entity/entity.c',
        ROOT/'src/entity/render/leaves.c',ROOT/'src/entity/construction/snapshots.c']
 paths=[p for p in paths if p.is_file()]
 return {'executable':digest(executable),'resources':tree(resources),'packet':tree(HERE),
         'selected_native_sources':{p.relative_to(ROOT).as_posix():digest(p) for p in paths},
         'particle_sources':tree(ROOT/'src/entity/particles')}

def native_constructors(rows):
 K.require(len(rows)==38,'all37 scenario constructors plus one independent FP control constructor')
 for row in rows:
  K.require(type(row) is dict and set(row)=={'kind','math_values','own_values','state'},'complete native constructor row')
  K.constructor_fields(row['state'])
  K.A.formula(row['state'],row['kind'],row['math_values'],row['own_values'])
 return {'constructors':38,'own_shared_doubles':380,'own_private_floats':152,
         'supplied_game_entropy':False,'scope':'native own values checked against shadow LCG in C; source own streams audited separately'}

def controls(expected,constructors):
 total=0
 def reject(call):
  nonlocal total
  try:call()
  except (ValueError,KeyError,TypeError,IndexError):total+=1;return
  raise ValueError('redstone comparison accepted a changed observation')
 for changed in (expected[:-1],expected+[expected[-1]],[expected[1],expected[0],*expected[2:]]):reject(lambda changed=changed:K.P.compare(changed,expected))
 for key,value in (('profile',True),('name','changed'),('kind','other'),('operation','other'),('exception','other'),('brightness','00000000'),('nw',{}),('packed',['00000000'])):
  changed=copy.deepcopy(expected);changed[0][key]=value;reject(lambda changed=changed:K.P.compare(changed,expected))
 for which in ('before','after'):
  for key in expected[0][which]:
   changed=copy.deepcopy(expected);changed[0][which][key]=None
   reject(lambda changed=changed:K.P.compare(changed,expected))
 for key in ('math_values','own_values'):
  changed=copy.deepcopy(constructors);changed[0][key]=changed[0][key][:-1]
  reject(lambda changed=changed:native_constructors(changed))
 for key in ('velocity','rgb'):
  changed=copy.deepcopy(constructors);changed[0]['state'][key][0]='0000000000000000' if key=='velocity' else '00000000'
  reject(lambda changed=changed:native_constructors(changed))
 return total

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--work-dir',type=Path,required=True)
 args=parser.parse_args();executable=args.executable.resolve(strict=True);args.work_dir.mkdir(parents=True,exist_ok=True)
 output=Path(tempfile.mkdtemp(prefix='redstone-particle-',dir=args.work_dir.resolve()));resources=output/'resources';game=output/'game';game.mkdir();R.resources_at(resources)
 before=snapshot(executable,resources);report={'status':'FAIL','inputs_before':before,'scope':'30 exact original component records plus independent native/original entropy; no default-seed identity claim'}
 try:
  report['original']=K.qualify();expected=K.reference_components()
  argv=[str(executable),str(resources),str(game),str(HERE/'profiles.tsv')];report['argv']=argv
  result=subprocess.run(argv,cwd=output,capture_output=True,timeout=180)
  (output/'native.stdout').write_bytes(result.stdout);(output/'native.stderr').write_bytes(result.stderr)
  report.update(returncode=result.returncode,stdout_sha256=hashlib.sha256(result.stdout).hexdigest(),stderr_sha256=hashlib.sha256(result.stderr).hexdigest())
  if result.returncode or result.stderr:raise ValueError('native process failed: '+result.stderr.decode(errors='backslashreplace'))
  text=result.stdout.decode('ascii');marker=r'REDSTONE_PARTICLE_NATIVE constructors=38 ticks=12 draws=12 brightness=6 own-unchanged-words=25165440 fp-admission=1 cleanup=1 checks=[1-9][0-9]*'
  K.require(re.fullmatch(marker,text.splitlines()[-1]),'complete actual native ownership/tail/floating receipt')
  actual=K.P.rows(text,'REDSTONE_PARTICLE_COMPONENT');K.P.compare(actual,expected)
  ctors=K.P.rows(text,'REDSTONE_PARTICLE_NATIVE_CONSTRUCTOR');report['own_native_entropy']=native_constructors(ctors)
  report['corruption_controls']=controls(expected,ctors);report['compared_components']=len(actual);report['status']='PASS'
 finally:
  report['inputs_after']=snapshot(executable,resources);report['inputs_unchanged']=report['inputs_before']==report['inputs_after']
  if not report['inputs_unchanged']:report['status']='FAIL'
  (output/'run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
 if report['status']!='PASS':raise ValueError('selected replay input continuity')
 print('PASS:30 exact original Redstone particle component rows;38 independently audited native constructors;'+str(report['corruption_controls'])+' corruption controls;'+str(output))

if __name__=='__main__':main()
