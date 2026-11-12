"""Authored fresh-root native SaveFormat controls; no JAR/JDK/game execution."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile

ROOT=Path(__file__).resolve().parents[3]
CAPTURE=ROOT/'tests/reference/client/notice/capture.py'
CAPTURE_SHA='4355d5e1f253355d239c6938762f05235b47059b4c3e8beb6e8306b4b34381e2'

def require(value,message):
    if not value:
        raise RuntimeError(message)

def pin(path):
    data=path.read_bytes()
    return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',required=True,type=Path)
    parser.add_argument('--work-dir',required=True,type=Path)
    args=parser.parse_args()
    executable=args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True,exist_ok=True)
    parent=args.work_dir.resolve(strict=True)
    require(parent.is_dir() and not parent.is_symlink(),'actual owned existing work directory')
    root=Path(tempfile.mkdtemp(prefix='save-format-authored-',dir=parent)).resolve()
    fixture=root/'saves'
    require(not fixture.exists(),'fresh absent authored fixture root')
    require(pin(CAPTURE)['sha256']==CAPTURE_SHA,'reviewed shared complete process capture')
    spec=importlib.util.spec_from_file_location('save_format_capture',CAPTURE)
    capture=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    before=pin(executable)
    env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1')
    report={'scope':'authored real native File/Source prefix/retirement controls; no original game',
            'root':str(root),'executable_before':before,'passed':False}
    try:
        result=capture.capture([str(executable),'--bad-input'],root,'arguments',30,env)
        out=(root/'arguments.stdout').read_bytes()
        err=(root/'arguments.stderr').read_bytes()
        require(type(result['exit']) is int and result['exit']==2 and result['reader_errors']=={} and
                result['pipe_capture_complete'] and not result['timed_out'] and not result['output_overflow'],
                'exact completed bad-argument exit2; no arbitrary crashes count as rejection')
        require(out==b'' and err in [b'SaveFormat fixture input: expected absent owned --fixture-root path\n',
                                   b'SaveFormat fixture input: expected absent owned --fixture-root path\r\n'],
                'exact sole bad-argument diagnostic')
        report['arguments']=result
        result=capture.capture([str(executable),'--fixture-root',str(fixture)],root,'native',180,env)
        capture.require_success(result,empty_stderr=True)
        out=(root/'native.stdout').read_bytes()
        match=re.fullmatch(rb'SaveFormat source models: ([1-9][0-9]*) checks;streaming body and explicit diagnostic/handler frontiers\r?\n',out)
        require(match is not None,'actual complete positive native footer')
        report['native']=result
        report['checks']=int(match[1])
        report['fixtures_after']={p.relative_to(root).as_posix():pin(p)
                                  for p in sorted(fixture.rglob('*')) if p.is_file()}
        require(not any(p.name=='session.lock' for p in fixture.rglob('*')),
                'list/info/open-frontier did not write session.lock')
        require(pin(executable)==before,'actual native executable bytes unchanged')
        report['passed']=True
    finally:
        report['executable_after']=pin(executable)
        (root/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
        print('SaveFormat artifacts: '+str(root))
    print('PASS: authored SaveFormat process/ownership/source-prefix controls')

if __name__=='__main__':
    main()
