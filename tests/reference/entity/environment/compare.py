"""Require completed bounded evidence before exact original/native comparison."""
import argparse,json,re
from pathlib import Path
from capture import ensure,digest,require_success
from run_native import validate_report
TAGS=re.compile(r'^(WATER|LAVA|FLAG|CLEAR|REL|PUBLISHED|PREFIX) ')

def original(path,side,marker):
    report=json.loads(path.read_text());require_success(report,empty_stderr=True)
    ensure(report['kind']=='original_environment_'+side,'original side differs')
    raw=path.parent/report['stdout_file'];err=path.parent/report['stderr_file']
    ensure(digest(raw)==report['stdout_sha256'] and digest(err)==report['stderr_sha256'] and err.stat().st_size==0,'original capture changed')
    lines=raw.read_text().splitlines();end=re.fullmatch(marker+r': ([1-9][0-9]*) rows',lines[-1]) if lines else None
    rows=[line for line in lines if TAGS.match(line)]
    ensure(end is not None and len(rows)==report['records']==int(end.group(1)),'incomplete original rows')
    return rows
def exact(label,expected,actual):
    ensure(len(expected)==len(actual),label+' cardinality differs')
    differences=[f'{i}: original={a} native={b}' for i,(a,b) in enumerate(zip(expected,actual)) if a!=b]
    ensure(not differences,label+'\n'+'\n'.join(differences[:24]))
def main(argv=None):
    p=argparse.ArgumentParser();p.add_argument('--native-run',type=Path,required=True)
    p.add_argument('--client-run',type=Path,required=True);p.add_argument('--server-run',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args(argv)
    report,path=validate_report(a.native_run);rows=[line for line in path.read_text().splitlines() if TAGS.match(line)]
    client=original(a.client_run,'client','Original entity environment');server=original(a.server_run,'server','Original server relative')
    exact('client',client,[line for line in rows if line.split()[1]=='0'])
    exact('server relative/prefixes',server,[line for line in rows if line.split()[1]=='1' and line.split()[0] in ('REL','PREFIX')])
    shared=[line.replace(' 0 ',' 1 ',1) for line in client if line.split()[0] in ('WATER','LAVA','FLAG','CLEAR') and int(line.split()[2])<3]
    exact('shared server environment',shared,[line for line in rows if line.split()[1]=='1' and line.split()[0] in ('WATER','LAVA','FLAG','CLEAR')])
    ensure(len(rows)==len(client)+len(server)+len(shared),'uncompared native records')
    result={'status':'PASS','client_records':len(client),'original_server_records':len(server),
        'native_shared_server_environment_records':len(shared),'native_checks':report['checks'],
        'scope':'Unchanged client full helper observations; unchanged server relative/null-World prefix observations; paired code pins/native crosscheck for successful server World helpers'}
    a.output.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(result))
if __name__=='__main__':main()
