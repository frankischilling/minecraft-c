"""UNRUN JAR/JDK-free exact CLI rejection controls over the actual probe."""
import argparse,hashlib,importlib.util,json,os,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
CAPTURE=ROOT/'tests/reference/client/notice/capture.py'
def require(ok,message):
    if not ok:raise RuntimeError(message)
def pin(path):
    raw=path.read_bytes();return {'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--executable',type=Path,required=True);parser.add_argument('--work-dir',type=Path,required=True);args=parser.parse_args();executable=args.executable.resolve(strict=True);require(executable.is_file(),'actual native product');before=pin(executable)
    require(pin(CAPTURE)['sha256']=='4355d5e1f253355d239c6938762f05235b47059b4c3e8beb6e8306b4b34381e2','reviewed complete capture')
    spec=importlib.util.spec_from_file_location('stream_probe_capture',CAPTURE);capture=importlib.util.module_from_spec(spec);spec.loader.exec_module(capture)
    args.work_dir.mkdir(parents=True,exist_ok=True);work=Path(tempfile.mkdtemp(prefix='stream-probe-inputs-',dir=args.work_dir.resolve()));fixture=work/'never-opened';env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1');rows=[[],['ij'],['as',str(fixture),'extra'],['unknown',str(fixture)],['ij ',str(fixture)],['ij\n',str(fixture)]];report={'status':'RUNNING','executable_before':before,'processes':[]}
    try:
        for i,row in enumerate(rows):
            stem='input-'+str(i);r=capture.capture([str(executable),*row],work,stem,30,env);report['processes'].append(r)
            require(type(r['exit']) is int and r['exit']==2 and r['reader_errors']=={} and r['pipe_capture_complete'] and not r['timed_out'] and not r['output_overflow'],'exact completed exit2, never arbitrary crash-as-rejection')
            require((work/(stem+'.stdout')).read_bytes()==b'' and (work/(stem+'.stderr')).read_bytes() in (b'Source stream trace input: expected ij|as|gzip and fixture path\n',b'Source stream trace input: expected ij|as|gzip and fixture path\r\n'),'exact sole diagnostic');require(not fixture.exists(),'input rejected before real fixture operation')
        require(pin(executable)==before,'same native product after all controls');report['status']='PASS'
    finally:
        report['executable_after']=pin(executable);(work/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');print('Stream probe artifacts: '+str(work))
    print('PASS: 6 exact native input controls')
if __name__=='__main__':main()
