"""Publish completed original Furnace evidence after full retained transport readback."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


A = module('furnace_publication_audit', HERE / 'audit.py')
P = module('furnace_publication_projection', HERE / 'projection.py')
C = module('furnace_publication_capture', HERE / 'capture.py')


def captured_process(directory, record, stem):
    """Read recorded bounded transport, without starting a child process."""
    A.require(type(record) is dict and type(record.get('exit')) is int and record['exit'] == 0 and
              record.get('timed_out') is False and record.get('pipe_capture_complete') is True and
              record.get('reader_errors') == {} and record.get('output_overflow') is False,
              'completed recorded process: ' + stem)
    A.require(type(record.get('timeout_seconds')) is int and 1 <= record['timeout_seconds'] <= 300,
              'bounded recorded process: ' + stem)
    result = {}
    for stream, limit in (('stdout', C.C.STDOUT_LIMIT), ('stderr', C.C.STDERR_LIMIT)):
        name = stem + '.' + stream
        A.require(record.get(stream + '_file') == name and record.get(stream + '_limit') == limit,
                  'exact recorded stream identity: ' + name)
        path = directory / name
        A.require(path.is_file() and not path.is_symlink() and path.stat().st_size <= limit,
                  'bounded recorded stream: ' + name)
        with path.open('rb') as source:
            raw = source.read(limit + 1)
        A.require(len(raw) <= limit and type(record.get(stream + '_bytes')) is int and
                  record[stream + '_bytes'] == len(raw) and
                  record.get(stream + '_sha256') == hashlib.sha256(raw).hexdigest(),
                  'recorded stream byte count and hash: ' + name)
        result[stream] = raw
    return result


def validate_transport(directory, report, cases):
    for key, stem in (('compile','compile'), ('java_version','java-version'), ('javac_version','javac-version')):
        streams = captured_process(directory, report[key], stem)
        if key in ('compile','javac_version'):
            A.require(not streams['stderr'], 'clean compiler transport')
    for stem in ('original','original-repeat'):
        streams = captured_process(directory, report[stem], stem)
        A.require(streams['stderr'].decode('utf8').splitlines() == C.expected_warnings(directory/'classes'), 'declared original target warnings')
        A.require(C.read_rows(streams['stdout'], cases, report['pins']) == report['observations'], 'report equals reparsed ordinary stream: '+stem)
    for stem in ('trace-interpreter','trace-default'):
        streams = captured_process(directory, report[stem], stem)
        A.require(not streams['stderr'], 'clean debugger host transport')
        target = directory/(stem+'-jdi')
        artifacts = report['trace_artifacts'][stem]
        A.require(set(artifacts) == {'target.stdout','target.stderr','calls.ndjson','live-Code.json'}, 'complete debugger artifact set')
        raw = {}
        for name,pin in artifacts.items():
            path=target/name
            A.require(path.is_file() and not path.is_symlink() and path.stat().st_size<=4194304, 'bounded actual debugger artifact')
            raw[name]=path.read_bytes()
            A.require(pin == {'bytes':len(raw[name]),'sha256':hashlib.sha256(raw[name]).hexdigest()}, 'actual debugger artifact count/hash')
        A.require(raw['target.stderr'].decode('utf8').splitlines() == C.expected_warnings(directory/'classes'), 'declared debugger target warnings')
        A.require(C.read_rows(raw['target.stdout'],cases,report['pins']) == report['observations'], 'report equals reparsed debugger target')
        rows,code = C.read_trace(target,cases,report['pins'])
        A.require(rows == report['traces'][stem] and code == report['live_Code'][stem], 'report equals actual source journal/live Code')
        summary=f'FURNACE_TRACE_COMPLETE rows={len(cases)} entries={sum(len(row["entries"]) for row in rows)} live-code={len(code)}'
        A.require(streams['stdout'].decode('utf8').splitlines()==[summary], 'complete debugger journal summary')
    A.require(report['traces']['trace-interpreter']==report['traces']['trace-default'] and report['live_Code']['trace-interpreter']==report['live_Code']['trace-default'], 'both passive source traces agree literally')
    A.validate(cases,report['observations'])
    A.validate_traces(cases,report['traces']['trace-interpreter'],report['live_Code']['trace-interpreter'],report['pins'])


def validate_capture(directory):
    report=A.load(directory/'capture.json')
    A.require(report['status']=='PASS' and report['inputs_before']==report['inputs_after'] and report['compiled_before']==report['compiled_after'], 'immutable completed original capture')
    for name,pin in report['inputs_before'].items():
        A.require(A.digest(name)==pin, 'actual captured input still unchanged: '+name)
    for name,pin in report['compiled_before'].items():
        A.require(A.digest(directory/name)==pin,'actual compiled observer still unchanged')
    for name in A.EXECUTION:
        A.require(report['inputs_before'][str((HERE/name).resolve())]==A.digest(HERE/name), 'exact execution helper')
    cases=A.load(HERE/'cases.json')['cases']
    A.require((directory/'declared-inputs.tsv').read_bytes()==C.declarations(cases).encode('utf8'), 'actual declaration transport equals pinned cases')
    jars=[Path(name) for name,pin in report['inputs_before'].items() if pin==A.JAR]
    A.require(len(jars)==1,'one unchanged original archive')
    with zipfile.ZipFile(jars[0]) as archive:
        pins={'client_jar_sha256':A.JAR,'classes':{name:C.I.inspect_class(archive.read(name+'.class')) for name in C.CORE}}
    A.require(pins==report['pins'],'reported Code pins equal actual unchanged original archive')
    validate_transport(directory,report,cases)
    return report,cases,jars[0]


def write(name,value):
    (HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True)
    args=parser.parse_args()
    directory=args.capture.resolve(strict=True)
    report,cases,jar=validate_capture(directory)
    scanner=module('furnace_caller_inventory',HERE/'scan_callers.py')
    callers=scanner.scan(jar)
    # All validation above and caller rederivation finish before public writes.
    write('finite.json',{'schema':1,'client_jar_sha256':A.JAR,
        'scope':'Genuine original SK/IX/IZ/EY/NBT and BM, unbound World/Player; no completed TC/tick, concrete Player, GUI or gameplay qualification.',
        'observations':report['observations']})
    write('source-pins.json',report['pins'])
    write('traces.json',{'schema':1,'client_jar_sha256':A.JAR,'scope':'Actual selected original MethodEntry events, typed arguments/receivers and nearest selected source caller; no method-exit or heap-completeness claim.',
        'rows':report['traces']['trace-interpreter'],'live_Code':report['live_Code']['trace-interpreter']})
    write('caller-inventory.json',callers)
    generated=P.header(cases,report['observations']['rows'])
    path=HERE/'native-observations.h'
    if not path.exists() or P.tokens(path.read_text())!=P.tokens(generated):
        path.write_text(generated,encoding='utf8',newline='\n')
    preparations=[]
    for earlier in sorted(directory.parent.glob('capture-*/capture.json')):
        if earlier.parent!=directory:
            preparations.append({'id':earlier.parent.name,'bytes':earlier.stat().st_size,'sha256':A.digest(earlier),'status':A.load(earlier).get('status'),'scope':'Retained preparation, not part of final four-target cohort'})
    write('provenance.json',{'schema':1,'client_jar_sha256':A.JAR,
        'capture':{'id':directory.name,'sha256':A.digest(directory/'capture.json'),'status':report['status'],'ordinary_targets':2,'traced_targets':2,'cases_per_target':len(cases)},
        'execution_helpers':{name:A.digest(HERE/name) for name in A.EXECUTION},
        'publication_helpers':{name:A.digest(HERE/name) for name in A.PUBLICATION},
        'outputs':{name:A.digest(HERE/name) for name in A.OUTPUTS},
        'inputs_before':report['inputs_before'],'inputs_after':report['inputs_after'],
        'compiled_before':report['compiled_before'],'compiled_after':report['compiled_after'],
        'processes':{name:report[name] for name in ('compile','java_version','javac_version','original','original-repeat','trace-interpreter','trace-default')},
        'trace_artifacts':report['trace_artifacts'],'preparations':preparations,
        'publication_scope':'Later readback/generation only, no game execution. Both ordinary and both traced streams, journals/live Code and current retained input/product hashes rechecked before public writes.'})
    print(A.audit())


if __name__=='__main__':
    main()
