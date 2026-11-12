"""Admission of an actual successful Native target build and selected output.

Repeated authoring executions are allowed. The receipt must bind current source
and the selected current output before this qualified execution/comparison.
"""
import importlib.util,json,re
from pathlib import Path
BASE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('overlay_build_original',BASE/'original.py');original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
need=original.need
TARGET='mc_test_living_overlay_draw'
REQUIRED={'src/client/render/living_draw/overlay.c','src/client/render/living_draw/body.c','src/client/render/living_draw/animals.c','src/client/render/living_draw/creatures.c','src/client/render/living_draw/internal.h','src/client/render/living_draw_internal.h','src/client/display/display.c','src/client/display/sdk_staged.inc','src/client/display/source_surface.inc','tests/client/render/test_living_overlay_draw.c','tests/client/render/test_creature_draw.c','tests/client/startup/test_application_startup.c','cmake/client_mob_draw.cmake'}


def successful(data,executable):
    need(data.get('status')=='PASS','actual successful build status')
    phases=data.get('phases');need(type(phases) is list and phases,'nonempty actual build phases');found=False
    for phase in phases:
        need(type(phase.get('exit')) is int and phase['exit']==0,'all actual build phases succeeded')
        cmd=phase.get('command');need(type(cmd) is list and cmd and all(type(w) is str and w for w in cmd),'actual complete build argv')
        if '--build' in cmd and '--target' in cmd:
            target=[]
            for word in cmd[cmd.index('--target')+1:]:
                if word.startswith('-'):break
                target.append(word)
            need(target and all(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_.-]*',w) for w in target),'well-formed actual CMake targets')
            if TARGET in target:
                tree=original.physical(cmd[cmd.index('--build')+1]).resolve();need(executable.is_relative_to(tree),'actual selected output belongs to target build tree');found=True
        if cmd[0].replace('\\','/').rsplit('/',1)[-1].lower()=='msbuild.exe':
            solution=[w for w in cmd if w.lower().endswith('.slnx')];flags=[w for w in cmd if w.lower().startswith('/t:')]
            need(len(solution)==len(flags)==1,'actual single MSBuild solution/target list')
            target=flags[0][3:].split(';');need(target and len(target)==len(set(target)) and all(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_.-]*',w) for w in target),'well-formed actual MSBuild target list')
            need({'/p:configuration=debug','/p:platform=x64'} <= {w.lower() for w in cmd},'actual MSBuild Debug/x64 output')
            if TARGET in target:
                p=original.physical(solution[0]).resolve();need(p.is_file() and executable.parent==p.parent/'Debug','actual selected MSBuild output belongs to solution Debug tree');found=True
    need(found and executable.name in (TARGET,TARGET+'.exe'),'actual requested Living overlay target was built')


def admit(path,executable,data=None):
    path=path.resolve(strict=True);exe=executable.resolve(strict=True);observed=json.loads(path.read_text()) if data is None else data
    successful(observed,exe)
    before,after=observed.get('input_before_sha256'),observed.get('input_after_sha256')
    need(observed.get('inputs_unchanged') is True and type(before) is dict and before and before==after,'stable actual compiled source input map')
    original.fingerprints(before)
    needed={(original.ROOT/name).resolve():name for name in REQUIRED};found={needed[original.physical(name).resolve()] for name in before if original.physical(name).resolve() in needed}
    need(found==REQUIRED,'actual compiled map includes component/display/fixture and build registration')
    outputs=observed.get('output_sha256');need(type(outputs) is dict and outputs,'explicit actual selected target output pins')
    selected=[(name,pin) for name,pin in outputs.items() if original.physical(name).resolve()==exe]
    need(len(selected)==1 and original.sha(exe)==selected[0][1],'same actual selected executable output')
    paths={path:original.sha(path),exe:selected[0][1]}
    for phase in observed['phases']:
        if phase.get('log'):
            log=path.parent/phase['log'];need(log.is_file(),'actual build phase log');paths[log]=original.sha(log)
    paths.update({original.physical(name):pin for name,pin in before.items() if original.physical(name).is_file()})
    return {'receipt':path,'receipt_sha256':original.sha(path),'executable':exe,'executable_sha256':selected[0][1],'expected':paths,'source_input_count':len(before),'output_scope':observed.get('binding_scope','actual recorded target build/output'),'record':observed}


def recheck(admitted):
    for path,pin in admitted['expected'].items():need(path.is_file() and original.sha(path)==pin,'current compiled/executed input drift: '+str(path))


def negatives(path,executable):
    import copy
    data=json.loads(path.read_text());rejected=[]
    variants=[]
    bad=copy.deepcopy(data);bad['phases'][0]['exit']=1;variants.append(('failed_actual_build_phase',bad))
    bad=copy.deepcopy(data);bad['output_sha256']={};variants.append(('missing_selected_output',bad))
    bad=copy.deepcopy(data)
    for phase in bad['phases']:
        phase['command']=[word.replace(TARGET,TARGET+'_absent') for word in phase['command']]
    variants.append(('missing_actual_target',bad))
    bad=copy.deepcopy(data)
    for key in ['input_before_sha256','input_after_sha256']:
        bad[key]={name:pin for name,pin in bad[key].items() if original.physical(name).resolve()!=(original.ROOT/'src/client/render/living_draw/overlay.c').resolve()}
    variants.append(('missing_compiled_overlay_body',bad))
    for label,bad in variants:
        try:admit(path,executable,bad)
        except AssertionError:rejected.append(label)
        else:raise AssertionError('actual build validator admitted corruption: '+label)
    return rejected
