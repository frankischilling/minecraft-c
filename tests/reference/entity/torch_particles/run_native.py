"""Run genuine native owners and compare the exact original component subset."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result


K=module('torch_native_packet',HERE/'packet.py')
C=module('torch_native_capture',HERE.parents[1]/'client/controller/capture.py')
R=module('torch_native_resources',HERE.parents[1]/'client/player_statistics/support.py')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--mode',choices=('ticks','draw'),required=True)
    parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args()
    expected=K.qualify();executable=args.executable.resolve(strict=True);args.work_dir.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='torch-'+args.mode+'-',dir=args.work_dir.resolve()))
    R.resources_at(output/'resources');(output/'game').mkdir()
    inputs=[executable,HERE/'profiles.tsv',HERE/'run_native.py',HERE/'packet.py',HERE.parents[2]/'entity/particles/test_torch.c']
    if args.mode=='draw':inputs.append(HERE.parents[2]/'client/particles/test_torch_draw.c')
    # Current native source inputs are qualified separately from original data.
    repo=HERE.parents[3]
    inputs += [repo/p for p in ('src/entity/particles/smoke.c','src/entity/particles/flame.c','src/client/particles/render_item.c',
        'src/entity/render/leaves.c','src/entity/particles/particle_internal.h','include/mc/entity/particles/particle.h')]
    before={str(p):C.digest(p) for p in inputs}
    report={'status':'FAIL','inputs_before':before,'mode':args.mode}
    try:
        receipt=C.capture([str(executable),str(output/'resources'),str(output/'game'),str((HERE/'profiles.tsv').resolve())],output,'native',180,os.environ.copy())
        report['receipt']=receipt;C.require_success(receipt,empty_stderr=True)
        text=(output/'native.stdout').read_text();print(text,end='')
        marker=r'TORCH_PARTICLE_NATIVE ticks=12 constructors=6 failed-constructor=2 raw-retirement=2 lifecycle=2 cleanup=1 checks=[1-9][0-9]*' if args.mode=='ticks' else r'TORCH_PARTICLE_NATIVE draws=12 brightness=6 own-unchanged-words=25165440 fp-admission=2 cleanup=1 checks=[1-9][0-9]*'
        K.require(re.fullmatch(marker,text.splitlines()[-1]) is not None,'actual complete native ownership receipt')
        selected=K.P.rows(text,'TORCH_PARTICLE_COMPONENT')
        subset=[row for row in expected if (row['operation']=='tick')==(args.mode=='ticks')]
        K.P.compare(selected,subset)
        constructors=K.P.rows(text,'TORCH_PARTICLE_NATIVE_CONSTRUCTOR')
        if args.mode=='ticks':
            K.require([r['kind'] for r in constructors]==['smoke_default','smoke_scaled','large_smoke','smoke_zero','smoke_negative','flame'],'six independently audited actual native constructors')
            for row in constructors:K.A.formula(row['state'],row['kind'],row['math_values'],row['own_values'])
        else:K.require(constructors==[],'draw projection has no fabricated constructor oracle')
        report['rows']=len(selected);report['status']='PASS'
    finally:
        report['inputs_after']={str(p):C.digest(p) for p in inputs}
        if report['inputs_after']!=before:report['status']='FAIL'
        (output/'native-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    K.require(report['status']=='PASS','unchanged native producer/executable')
    print('Torch particles: exact original '+args.mode+' projection and independent native ownership/entropy controls')


if __name__=='__main__':main()
