"""Portable combined opt-in controller; each stage retains bounded evidence."""
import argparse,tempfile
from pathlib import Path
import compare,run_native,run_original

def main():
    p=argparse.ArgumentParser();p.add_argument('--executable',type=Path,required=True)
    p.add_argument('--client-jar',type=Path,required=True);p.add_argument('--server-jar',type=Path,required=True)
    p.add_argument('--libraries',type=Path,required=True);p.add_argument('--work-dir',type=Path,required=True)
    p.add_argument('--java');p.add_argument('--javac');a=p.parse_args()
    a.work_dir.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='entity-environment-reference-',dir=a.work_dir))
    run_native.main(['--executable',str(a.executable),'--output',str(output/'native')])
    options=['--client-jar',str(a.client_jar),'--server-jar',str(a.server_jar),
        '--libraries',str(a.libraries),'--output',str(output/'original')]
    if a.java:options.extend(['--java',a.java])
    if a.javac:options.extend(['--javac',a.javac])
    run_original.main(options)
    compare.main(['--native-run',str(output/'native/run.json'),
        '--client-run',str(output/'original/client-run.json'),'--server-run',str(output/'original/server-run.json'),
        '--output',str(output/'comparison.json')])
    print('Combined report: '+str((output/'comparison.json').resolve()))
if __name__=='__main__':main()
