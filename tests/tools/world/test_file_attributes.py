"""Authored real-File controls, including long native paths; no JAR/JDK/device."""
import argparse,hashlib,importlib.util,json,os,re,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
CAPTURE=ROOT/'tests/reference/client/notice/capture.py'
def require(ok,message):
    if not ok:raise RuntimeError(message)
def pin(path):
    raw=path.read_bytes();return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args();executable=args.executable.resolve(strict=True);require(executable.is_file(),'real executable');before=pin(executable)
    require(pin(CAPTURE)['sha256']=='4355d5e1f253355d239c6938762f05235b47059b4c3e8beb6e8306b4b34381e2','reviewed complete capture helper')
    spec=importlib.util.spec_from_file_location('file_attribute_capture',CAPTURE);capture=importlib.util.module_from_spec(spec);spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True,exist_ok=True);parent=args.work_dir.resolve(strict=True);require(not parent.is_symlink(),'owned real work parent');work=Path(tempfile.mkdtemp(prefix='file-attributes-',dir=parent));fixture=work/'files';env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1');report={'status':'RUNNING','executable_before':before,'processes':[]}
    try:
        r=capture.capture([str(executable),'bad','extra'],work,'arguments',30,env);report['processes'].append(r)
        require(type(r['exit']) is int and r['exit']==2 and r['reader_errors']=={} and r['pipe_capture_complete'] and not r['timed_out'] and not r['output_overflow'],'exact completed exit2, never crash-as-rejection')
        require((work/'arguments.stdout').read_bytes()==b'' and (work/'arguments.stderr').read_bytes() in (b'File attributes input: expected absent owned fixture root\n',b'File attributes input: expected absent owned fixture root\r\n'),'exact sole input diagnostic');require(not fixture.exists(),'fresh absent fixture root')
        r=capture.capture([str(executable),str(fixture)],work,'native',120,env);report['processes'].append(r);capture.require_success(r,empty_stderr=True)
        match=re.fullmatch(rb'File attributes: ([1-9][0-9]*) checks; independent real queries and native lifetime\r?\n',(work/'native.stdout').read_bytes());require(match is not None,'complete positive native footer');report['checks']=int(match[1]);require(pin(executable)==before,'same executable after controls')
        if os.name=='nt':
            alias=work/'alias'
            units=lambda p:len(str(p).encode('utf-16-le'))//2
            suffix=Path('absent. ')/'leaf'
            while units(alias/suffix)<247:
                remaining=247-units(alias/suffix);count=min(60,remaining-1)
                if remaining-count-1==1:count-=1
                require(count>0,'room for alias boundary component');alias/='a'*count
            alias/=suffix
            require(units(alias)==247 and not alias.exists(),'fresh 247-unit internal alias')
            r=capture.capture([str(executable),str(alias),'absent-alias'],work,'alias',120,env)
            report['processes'].append(r);capture.require_success(r,empty_stderr=True)
            require((work/'alias.stdout').read_bytes() in (b'File alias: successful missing-path query\n',b'File alias: successful missing-path query\r\n'),'complete alias footer')
        for target,relative in ((247,False),(248,False),(260,False),(400,False),(400,True)):
            long_root=work/('long-'+str(target)+('-relative' if relative else '')+'-caf\u00e9-\U0001f600')
            units=lambda p:len(str(p).encode('utf-16-le'))//2
            while units(long_root)<target:
                remaining=target-units(long_root)
                require(remaining>=2,'room for bounded native path component')
                count=min(60,remaining-1)
                if remaining-count-1==1:count-=1
                long_root/='a'*count
            require(units(long_root)==target and not long_root.exists(),'exact fresh UTF16 root boundary')
            label='native-long-'+str(target)+('-relative' if relative else '')
            spelling=os.path.relpath(long_root,parent).replace(os.sep,'/') if relative else str(long_root)
            original_cwd=Path.cwd()
            try:
                if relative:os.chdir(parent)
                r=capture.capture([str(executable),spelling],work,label,120,env)
            finally:
                os.chdir(original_cwd)
            report['processes'].append(r);capture.require_success(r,empty_stderr=True)
            match=re.fullmatch(rb'File attributes: ([1-9][0-9]*) checks; independent real queries and native lifetime\r?\n',(work/(label+'.stdout')).read_bytes())
            require(match is not None and int(match[1])==report['checks'],'same complete native controls at long root')
            report.setdefault('long_roots',[]).append({'units':target,'relative':relative,'checks':int(match[1])})
        require(pin(executable)==before,'same executable after long path controls')
        report['fixtures']={p.relative_to(fixture).as_posix():pin(p) for p in fixture.rglob('*') if p.is_file()};require(not any(p.name=='session.lock' for p in fixture.rglob('*')),'no World/session side effect');report['status']='PASS'
    finally:
        report['executable_after']=pin(executable);(work/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');print('File attribute artifacts: '+str(work))
    print('PASS: real File attribute/native-lifetime controls')
if __name__=='__main__':main()
