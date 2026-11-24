"""Publish completed original World Furnace lifecycle evidence after full retained transport readback."""
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


A = module('furnace_lifecycle_publication_audit', HERE / 'audit.py')
P = module('furnace_lifecycle_publication_projection', HERE / 'projection.py')
C = module('furnace_lifecycle_publication_capture', HERE / 'capture.py')


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


def artifact(path,pin):
    A.require(path.is_file() and not path.is_symlink() and path.stat().st_size<=4194304,'bounded retained original artifact')
    raw=path.read_bytes()
    A.require(pin=={'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()},'actual retained artifact count/hash')
    return raw


def validate_transport(directory,report,declaration):
    for key,stem in (('compile','compile'),('java_version','java-version'),('javac_version','javac-version')):
        streams=captured_process(directory,report[key],stem)
        if key!='java_version':A.require(not streams['stderr'],'clean original compiler transport')
    for stem in A.MODES:
        streams=captured_process(directory,report[stem],stem)
        if stem.startswith('trace-'):
            A.require(not streams['stderr'],'clean debugger host transport')
            target=directory/(stem+'-jdi');pins=report['trace_artifacts'][stem]
            A.require(set(pins)=={'target.stdout','target.stderr','phases.ndjson','live-Code.json'},'complete phase artifact set')
            raw={name:artifact(target/name,pin) for name,pin in pins.items()}
            stdout,stderr=raw['target.stdout'],raw['target.stderr']
            phases,code=C.read_trace(target,declaration,report['pins'])
            A.require(phases==report['traces'][stem] and code==report['live_Code'][stem],'report equals actual phase journal/live Code')
            summary=f'FURNACE_WORLD_TRACE_COMPLETE rows={len(declaration["steps"])} phases={sum(len(r["phases"]) for r in phases)} live-code={len(code)}'
            A.require(streams['stdout'].decode('utf8').splitlines()==[summary],'complete original phase summary')
        else:stdout,stderr=streams['stdout'],streams['stderr']
        A.require(stderr.decode('utf8').splitlines()==C.expected_warnings(directory/'classes'),'only exact original guard warnings')
        rows,entropy,completion,setup=C.read_output(stdout,declaration,report['pins'])
        A.require(rows==report['rows'] and entropy==report['entropy'][stem] and completion==report['completion'][stem] and setup==report['setup'][stem],'report equals reparsed original stream: '+stem)
    A.require(report['traces']['trace-interpreter']==report['traces']['trace-default'] and report['live_Code']['trace-interpreter']==report['live_Code']['trace-default'],'both actual phase journals agree')
    A.require(all(value==report['setup']['interpreter'] for value in report['setup'].values()),'all four actual prepared Worlds agree')
    A.validate(declaration,report['rows']);A.validate_random(declaration,report['entropy'])
    A.validate_traces(declaration,report['traces']['trace-interpreter'],report['live_Code']['trace-interpreter'],report['pins'])


def validate_capture(directory):
    report=A.load(directory/'capture.json')
    A.require(report['status']=='PASS' and report['inputs_before']==report['inputs_after'] and report['compiled_before']==report['compiled_after'],'immutable completed original lifecycle cohort')
    for name,pin in report['inputs_before'].items():A.require(A.digest(name)==pin,'actual original input/helper still unchanged: '+name)
    for name,pin in report['compiled_before'].items():A.require(A.digest(directory/name)==pin,'compiled original observer still unchanged')
    for name in A.EXECUTION:A.require(report['inputs_before'][str((HERE/name).resolve())]==A.digest(HERE/name),'exact captured public execution helper')
    declaration=A.load(HERE/'cases.json')
    A.require((directory/'profiles.tsv').read_bytes()==C.declarations(declaration).encode('utf8'),'actual declaration transport equals pinned inputs')
    jars=[Path(name) for name,pin in report['inputs_before'].items() if pin==A.JAR];A.require(len(jars)==1,'one actual unchanged original JAR')
    with zipfile.ZipFile(jars[0]) as archive:
        pins={'client_jar_sha256':A.JAR,'classes':{name:C.I.inspect_class(archive.read(name+'.class')) for name in C.CORE}}
    A.require(pins==report['pins'],'all class/Code pins rederived from original archive')
    A.require(len(report['worlds_before'])==len(report['worlds_after'])==4 and all(v==report['worlds_before'][0] for v in report['worlds_before']),'four identical frozen saved inputs')
    for index,stem in enumerate(A.MODES):
        before,after=report['worlds_before'][index],report['worlds_after'][index]
        A.require(C.inventory(directory/stem/'world')==after and set(before)==set(after) and all(after[name]==value for name,value in before.items() if name!='session.lock') and after['session.lock']['bytes']==8,'only private session lock changed on actual disk')
    validate_transport(directory,report,declaration)
    return report,declaration


def write(name,value):
    (HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);args=parser.parse_args()
    directory=args.capture.resolve(strict=True);report,declaration=validate_capture(directory)
    # No public write precedes complete actual input/product/transport readback.
    write('finite.json',{'schema':1,'client_jar_sha256':A.JAR,'rows':report['rows']})
    write('setup.json',{'schema':1,'client_jar_sha256':A.JAR,'world':report['setup']['interpreter']})
    write('random.json',{'schema':1,'client_jar_sha256':A.JAR,'streams':report['entropy']})
    write('traces.json',{'schema':1,'client_jar_sha256':A.JAR,'rows':report['traces']['trace-interpreter'],'live_Code':report['live_Code']['trace-interpreter']})
    write('source-pins.json',report['pins'])
    generated=P.header(declaration,report['rows'],report['setup']['interpreter']);path=HERE/'native-observations.h'
    if not path.exists() or P.tokens(path.read_text())!=P.tokens(generated):path.write_text(generated,encoding='utf8',newline='\n')
    preparations=[{'id':v.parent.name,'bytes':v.stat().st_size,'sha256':A.digest(v),'status':A.load(v).get('status'),'scope':'Retained preparation, not final cohort'} for v in sorted(directory.parent.glob('capture-*/capture.json')) if v.parent!=directory]
    write('provenance.json',{'schema':1,'client_jar_sha256':A.JAR,'capture':{'id':directory.name,'sha256':A.digest(directory/'capture.json'),'status':report['status'],'ordinary_targets':2,'traced_targets':2,'steps_per_target':len(declaration['steps'])},
        'execution_helpers':{name:A.digest(HERE/name) for name in A.EXECUTION},'publication_helpers':{name:A.digest(HERE/name) for name in A.PUBLICATION},'outputs':{name:A.digest(HERE/name) for name in A.OUTPUTS},
        'inputs_before':report['inputs_before'],'inputs_after':report['inputs_after'],'compiled_before':report['compiled_before'],'compiled_after':report['compiled_after'],
        'processes':{name:report[name] for name in ('compile','java_version','javac_version',*A.MODES)},'trace_artifacts':report['trace_artifacts'],
        'worlds_before':report['worlds_before'],'worlds_after':report['worlds_after'],'completion':report['completion'],'preparations':preparations,
        'publication_scope':'Later readback/generation only. All four original target streams and both phase journals/live Code, actual saved inventories and retained input/product hashes rechecked before public writes.'})
    print(A.audit())


if __name__=='__main__':main()
