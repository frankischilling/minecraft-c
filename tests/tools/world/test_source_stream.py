"""UNRUN authored genuine-file stream tests. No JAR/JDK/device."""
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
def require(value,message):
    if not value:raise RuntimeError(message)
def pin(path):
    b=path.read_bytes();return {'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args()
    executable=args.executable.resolve(strict=True);require(executable.is_file(),'actual executable');before=pin(executable)
    require(pin(CAPTURE)['sha256']=='4355d5e1f253355d239c6938762f05235b47059b4c3e8beb6e8306b4b34381e2','reviewed process helper')
    spec=importlib.util.spec_from_file_location('source_stream_capture',CAPTURE);capture=importlib.util.module_from_spec(spec);spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True,exist_ok=True);parent=args.work_dir.resolve(strict=True);require(not parent.is_symlink(),'owned parent');work=Path(tempfile.mkdtemp(prefix='source-stream-authored-',dir=parent));env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1');report={'status':'RUNNING','input':before,'processes':[]}
    try:
        r=capture.capture([str(executable),'bad','extra'],work,'arguments',30,env);report['processes'].append(r)
        require(r['exit']==2 and r['reader_errors']=={} and r['pipe_capture_complete'] and not r['timed_out'] and not r['output_overflow'],'exact completed bad-input exit2, never a crash')
        require((work/'arguments.stdout').read_bytes()==b'' and (work/'arguments.stderr').read_bytes() in (b'Source stream input: expected absent owned fixture root\n',b'Source stream input: expected absent owned fixture root\r\n'),'exact sole input diagnostic')
        root=work/'files';require(not root.exists(),'fresh absent owned fixture root');r=capture.capture([str(executable),str(root)],work,'native',180,env);report['processes'].append(r);capture.require_success(r,empty_stderr=True)
        output=(work/'native.stdout').read_bytes();m=re.fullmatch(rb'Source stream: ([1-9][0-9]*) checks; real descriptor GZIP NBT and document identity\r?\n',output);require(m is not None,'complete positive footer');report['checks']=int(m[1]);require(pin(executable)==before,'executable unchanged')
        require(not any(p.name=='session.lock' for p in root.rglob('*')),'no Store/session lock side effect');report['files']={p.relative_to(root).as_posix():pin(p) for p in root.rglob('*') if p.is_file()};report['status']='PASS'
    finally:
        report['executable_after']=pin(executable);(work/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');print('Source stream artifacts: '+str(work))
    print('PASS: authored source-stream ownership and failure-prefix controls')
if __name__=='__main__':main()
