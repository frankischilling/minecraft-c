"""Compare full tagged records only after strict completed native evidence."""
import argparse,json,re
from pathlib import Path
from capture import ensure,digest,require_success
from run_native import validate_report

def main():
    p=argparse.ArgumentParser();p.add_argument('--native-run',type=Path,required=True)
    p.add_argument('--original-run',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();report,native=validate_report(a.native_run)
    oracle=json.loads(a.original_run.read_text());require_success(oracle,empty_stderr=True)
    raw=a.original_run.parent/oracle['stdout_file'];err=a.original_run.parent/oracle['stderr_file']
    ensure(digest(raw)==oracle['stdout_sha256'] and digest(err)==oracle['stderr_sha256'] and err.stat().st_size==0,'original capture changed')
    lines=raw.read_text().splitlines();match=re.fullmatch(r'Original material movement: ([1-9][0-9]*) rows',lines[-1]) if lines else None
    original=[x for x in lines if re.match(r'^(FLOW|SCAN|EYE|VEC|PLAYER) ',x)]
    ensure(match is not None and len(original)==int(match.group(1)),'original incomplete marker/count')
    rows=[x for x in native.read_text().splitlines() if re.match(r'^(FLOW|SCAN|EYE|VEC|PLAYER) ',x)]
    side0=[x for x in rows if x.split()[1]=='0']
    ensure(side0==original, '\n'.join(f'{i}: original={o} native={n}' for i,(o,n) in enumerate(zip(original,side0)) if o!=n) or 'record cardinality differs')
    side1=[x for x in rows if x.split()[1]=='1']
    shared=[x.replace(' 0 ',' 1 ',1) for x in original if not x.startswith('PLAYER ')]
    ensure(side1==shared,'server native shared records differ from client observations')
    result={'status':'PASS','client_original_records':len(original),'native_server_crosscheck_records':len(shared),
        'scope':'Unchanged client CPU oracle; separately pinned paired server bytecode, with native dual-side crosscheck. No original server process claim.',
        'native_checks':report['checks']}
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
