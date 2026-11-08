"""Bounded real kernel controls; explicit Tile2 adoption, no full SL claim."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
TIMEOUT = 240
LIMIT = 1024**2
MAX_CHECKS = 2**32 - 1
BUNDLE_SHA = '9e6f7b08c5294773f593727c016080c8e7233c858e2aeeb587ff8ed3696ddd6b'
MODES = ('empty', 'source', 'native', 'first-owner')
FIXTURE_MANIFEST = '94d5b87ba2ef91fc0f37fea967c27e47bc9bd39427be65ef1749204e828d6f6e'
PROTOCOL_SHA = '4f4e1ba34cbe81d0368cbb809a652923f6dafbacc5ba08412f43e20c22ca0d30'



def require(value, message):
    if not value:
        raise ValueError(message)

def literal_inputs(before, after, locks, require_locks):
    """Pure full-file guard; only exact eight-byte session.lock additions fit."""
    require(type(before) is dict and type(after) is dict and type(locks) is set and
            type(require_locks) is bool, 'Malformed literal input snapshots')
    for snapshot in (before, after):
        for name, value in snapshot.items():
            require(type(name) is str and name and type(value) is dict and
                    set(value) == {'bytes', 'sha256'} and type(value['bytes']) is int and
                    0 <= value['bytes'] <= LIMIT and type(value['sha256']) is str and
                    re.fullmatch(r'[0-9a-f]{64}', value['sha256']) is not None,
                    'Malformed full-file pin')
    require(all(name not in before and name.endswith('/session.lock') for name in locks),
            'Invalid declared source session paths')
    for name, value in before.items():
        require(after.get(name) == value, 'Authored source input changed or removed: ' + name)
    additions = set(after) - set(before)
    require(additions <= locks, 'Unexpected source output file')
    require(all(after[name]['bytes'] == 8 for name in additions), 'Source session token is not eight bytes')
    if require_locks:
        require(additions == locks, 'Missing actual source session token')

def verify_retained(child, byte_map):
    """Pure proof that all three retained payloads match ANY returned child."""
    names = {'native.stdout', 'native.stderr', 'native.capture.json'}
    require(type(byte_map) is dict and set(byte_map) == names, 'Missing retained native payload')
    require(all(type(value) is bytes and len(value) <= LIMIT for value in byte_map.values()),
            'Retained native payload type or extent')
    require(type(getattr(child, 'stdout', None)) is bytes and type(getattr(child, 'stderr', None)) is bytes and
            type(getattr(child, 'capture_report', None)) is dict, 'Missing actual returned child payload')
    require(byte_map['native.stdout'] == child.stdout and byte_map['native.stderr'] == child.stderr,
            'Retained native streams differ from returned bytes')

    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate retained capture JSON key')
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError('Nonstandard retained capture JSON constant: ' + value)

    def same_json(a, b):
        if type(a) is not type(b):
            return False
        if type(a) is dict:
            return set(a) == set(b) and all(type(key) is str and same_json(a[key], b[key]) for key in a)
        if type(a) is list:
            return len(a) == len(b) and all(same_json(x, y) for x, y in zip(a, b))
        return type(a) in (str, int, float, bool, type(None)) and a == b

    actual = json.loads(byte_map['native.capture.json'].decode('utf-8'),
                        object_pairs_hook=object_pairs, parse_constant=invalid_constant)
    require(type(actual) is dict and same_json(actual, child.capture_report),
            'Retained capture JSON differs from actual report')

def protocol_lines(mode):
    require(mode in MODES, 'Unknown kernel mode')
    markers = ('cold BEGIN', 'cold END', 'cache BEGIN', 'cache END') if mode == 'empty' else ('failure BEGIN', 'failure END')
    return [b'KERNEL_OWNER_BEGIN schema=1 mode=' + mode.encode('ascii'), b'16 achievements', b'151 recipes'] + [
        b'KERNEL_OWNER_CASE ' + marker.encode('ascii') for marker in markers]


def completed(child, mode, command):
    require(type(command) is list and len(command) == 3 and all(type(x) is str for x in command), 'Exact native command required')
    require(type(getattr(child, 'returncode', None)) is int and child.returncode == 0, 'Native exit is not integer success')
    require(type(getattr(child, 'stdout', None)) is bytes and type(getattr(child, 'stderr', None)) is bytes, 'Native streams are not bytes')
    c = getattr(child, 'capture_report', None)
    require(type(c) is dict, 'Missing capture report')
    require(type(c.get('command')) is list and c['command'] == command and type(getattr(child, 'args', None)) is list and child.args == command, 'Actual command receipt mismatch')
    require(type(c.get('exit')) is int and c['exit'] == 0 and c.get('reaped') is True and c.get('pipe_capture_complete') is True and
        c.get('timed_out') is False and c.get('output_overflow') is False and type(c.get('reader_errors')) is dict and c['reader_errors'] == {}, 'Failed/incomplete native capture')
    for name, value in [('timeout_seconds', TIMEOUT), ('limit', LIMIT), ('stdout_bytes',len(child.stdout)), ('stderr_bytes',len(child.stderr))]:
        require(type(c.get(name)) is int and c[name] == value, 'Wrong capture bound/count: ' + name)
    require(len(child.stdout) <= LIMIT and len(child.stderr) <= LIMIT and child.stderr == b'' and child.stdout.endswith(b'\n'), 'Native stream extent, stderr or final newline')
    normalized = child.stdout.replace(b'\r\n', b'\n')
    require(b'\r' not in normalized, 'Malformed newline')
    lines = normalized.split(b'\n')
    expected = protocol_lines(mode)
    require(lines[-1] == b'' and len(lines) == len(expected) + 2 and lines[:-2] == expected, 'Wrong exact mode/banner/control sequence')
    footer = re.fullmatch(rb'PASS: ([1-9][0-9]{0,9}) actual kernel execution checks;mode=' + mode.encode('ascii'), lines[-2])
    require(footer is not None, 'Missing/wrong literal completion footer')
    checks = int(footer.group(1))
    require(1 <= checks <= MAX_CHECKS, 'Unsigned native check count outside range')
    return checks


def bundle_payload(data):
    require(type(data) is bytes and len(data) <= LIMIT and hashlib.sha256(data).hexdigest() == BUNDLE_SHA, 'Unapproved explicit dependency bundle')
    bundle = json.loads(data.decode('utf-8'))
    require(type(bundle) is dict and bundle.get('schema') == 'EXPLICIT_KERNEL_EXECUTION_TILE2_BUNDLE_V1' and bundle.get('selection') == 'nested3+Entity2+kernel2+Tile2', 'Wrong selected dependency schema')
    require(bundle['fixture']['manifest']['sha256'] == FIXTURE_MANIFEST and bundle['fixture']['protocol']['sha256'] == PROTOCOL_SHA, 'Wrong immutable fixture/protocol supplier')
    return bundle


def literal_metadata(path):
    """Use lstat on Python3.10+, never silently skip Windows reparse checks."""
    metadata = path.lstat()
    require(not stat.S_ISLNK(metadata.st_mode), 'Literal path is a symbolic link')
    if os.name == 'nt':
        attributes = getattr(metadata, 'st_file_attributes', None)
        require(type(attributes) is int, 'Windows literal reparse metadata unavailable')
        require(attributes & 0x400 == 0, 'Literal path is a Windows reparse point')
    return metadata


def fixture_files(fixture_root):
    """Snapshot a real owned directory; a missing/replaced root is a failure."""
    require(isinstance(fixture_root, Path), 'Missing literal fixture root path')
    require(stat.S_ISDIR(literal_metadata(fixture_root).st_mode),
            'Literal fixture root is not a real directory')
    result = {}
    total = 0
    paths = sorted(fixture_root.rglob('*'))
    require(len(paths) <= 256, 'Fixture directory extent exceeded')
    for path in paths:
        metadata = literal_metadata(path)
        if stat.S_ISDIR(metadata.st_mode):
            continue
        require(stat.S_ISREG(metadata.st_mode) and metadata.st_size <= LIMIT,
                'Unexpected fixture file type/extent')
        value = pin(path)
        total += value['bytes']
        require(total <= 16 * LIMIT, 'Fixture total bytes exceeded')
        result[path.relative_to(fixture_root).as_posix()] = value
    require(stat.S_ISDIR(literal_metadata(fixture_root).st_mode),
            'Literal fixture root disappeared during snapshot')
    return result


def terminal_literal_guard(report, fixture_root, literal_before, protocol_complete):
    """Called unconditionally once a pre-run literal snapshot exists."""
    actual = fixture_files(fixture_root)
    report['literal_after'] = actual
    literal_inputs(literal_before, actual, {'world/session.lock'}, protocol_complete)
    report['literal_inputs_unchanged'] = True


def literal_root_self_test():
    """Python-only real directory controls, never Source data or a native child."""
    refused = 0
    temporary_parent = Path(tempfile.gettempdir()).resolve(strict=True)
    for operation in ('removed', 'renamed', 'replaced-empty'):
        with tempfile.TemporaryDirectory(prefix='kernel-terminal-root-', dir=temporary_parent) as name:
            base = Path(name).resolve(strict=True)
            # Validate the exact newly created cleanup target before the
            # TemporaryDirectory owner's eventual recursive disposal.
            require(base.parent == temporary_parent and base.name.startswith('kernel-terminal-root-'),
                    'Disposable Python control escaped its intended parent')
            owned = base / 'owned'
            owned.mkdir()
            literal = owned / 'literal.bin'
            literal.write_bytes(b'Python guard control only; no Source fixture')
            before = fixture_files(owned)
            positive = {}
            terminal_literal_guard(positive, owned, before, False)
            require(positive.get('literal_inputs_unchanged') is True, 'Real-directory guard positive')
            require(owned.resolve(strict=True).parent == base, 'Exact disposable root boundary')
            if operation == 'removed':
                literal.unlink()
                owned.rmdir()
            else:
                renamed = base / 'renamed'
                require(renamed.parent.resolve(strict=True) == base and not renamed.exists(),
                        'Exact disposable rename boundary')
                owned.rename(renamed)
                if operation == 'replaced-empty':
                    owned.mkdir()
            # This is the same terminal helper as main's finally path. A
            # simulated already-PASS envelope must be downgraded on refusal.
            report = {'status': 'PASS'}
            try:
                terminal_literal_guard(report, owned, before, False)
            except (ValueError, OSError):
                report['status'] = 'FAIL'
                refused += 1
            require(report['status'] == 'FAIL' and report.get('literal_inputs_unchanged') is not True,
                    'Missing/renamed/replaced literal root admitted')
    require(refused == 3, 'Every real whole-root mutation refused')
    return refused


def self_test():
    rejected = retention = literals = bundles = 0
    for mode in MODES:
        command = ['actual-native', mode, 'fresh-root']
        footer = b'PASS: 1 actual kernel execution checks;mode=' + mode.encode('ascii')
        stdout = b'\n'.join(protocol_lines(mode) + [footer]) + b'\n'
        report = dict(command=command.copy(), exit=0, reaped=True, pipe_capture_complete=True, timed_out=False,
            output_overflow=False, reader_errors={}, timeout_seconds=TIMEOUT, limit=LIMIT, stdout_bytes=len(stdout), stderr_bytes=0)
        good = SimpleNamespace(args=command.copy(), returncode=0, stdout=stdout, stderr=b'', capture_report=report)
        require(completed(good,mode,command)==1,'Pure protocol positive')
        windows = copy.deepcopy(good)
        windows.stdout = stdout.replace(b'\n',b'\r\n')
        windows.capture_report['stdout_bytes'] = len(windows.stdout)
        require(completed(windows,mode,command)==1,'Pure CRLF positive')
        bad = []
        for key,value in [('returncode',1),('returncode',False),('returncode',0.0),('stderr',b'real failure\n'),('stderr',''),('stdout',stdout[:-1]),
            ('stdout',stdout+footer+b'\n'),('stdout',stdout.decode()),('args',command[:-1]),('stdout',stdout.replace(b'16 achievements\n',b'')),
            ('stdout',stdout.replace(b'151 recipes\n',b'151 recipes\n16 achievements\n151 recipes\n')),('stdout',stdout.replace(b'KERNEL_OWNER_CASE',b'UNKNOWN_CASE',1)),
            ('stdout',stdout.replace(footer,footer.replace(b'PASS: 1 ',b'PASS: 0 '))),('stdout',stdout.replace(footer,footer.replace(b'PASS: 1 ',b'PASS: 01 '))),
            ('stdout',stdout.replace(footer,footer.replace(b'PASS: 1 ',b'PASS: 4294967296 '))),('stdout',stdout.replace(b'\n',b'\r',1))]:
            item = copy.deepcopy(good)
            setattr(item,key,value)
            if type(item.stdout) is bytes:item.capture_report['stdout_bytes']=len(item.stdout)
            if type(item.stderr) is bytes:item.capture_report['stderr_bytes']=len(item.stderr)
            bad.append(item)
        for key,value in [('exit',True),('exit',1),('exit',0.0),('reaped',False),('reaped',1),('pipe_capture_complete',False),('pipe_capture_complete',1),
            ('timed_out',True),('timed_out',0),('output_overflow',True),('output_overflow',0),('reader_errors',[]),('reader_errors',{'stdout':'lost'}),
            ('command',command[:-1]),('command',tuple(command)),('timeout_seconds',True),('timeout_seconds',1),('limit',True),('limit',LIMIT+1),
            ('stdout_bytes',False),('stdout_bytes',len(stdout)+1),('stderr_bytes',False)]:
            item=copy.deepcopy(good);item.capture_report[key]=value;bad.append(item)
        swapped=copy.deepcopy(good)
        rows=protocol_lines(mode);rows[-1],rows[-2]=rows[-2],rows[-1]
        swapped.stdout=b'\n'.join(rows+[footer])+b'\n';swapped.capture_report['stdout_bytes']=len(swapped.stdout);bad.append(swapped)
        wrong=copy.deepcopy(good)
        wrong.stdout=stdout.replace(b'mode='+mode.encode(),b'mode=wrong');wrong.capture_report['stdout_bytes']=len(wrong.stdout);bad.append(wrong)
        oversized=copy.deepcopy(good);oversized.stdout=b'x'*(LIMIT+1);oversized.capture_report['stdout_bytes']=len(oversized.stdout);bad.append(oversized)
        missing=copy.deepcopy(good);missing.capture_report=None;bad.append(missing)
        for item in bad:
            try:completed(item,mode,command)
            except ValueError:rejected+=1;continue
            raise ValueError('Corrupt protocol accepted: '+mode)
        def payloads(child):
            return {'native.stdout':child.stdout,'native.stderr':child.stderr,'native.capture.json':json.dumps(child.capture_report).encode()+b'\n'}
        verify_retained(good,payloads(good))
        red=copy.deepcopy(good);red.returncode=red.capture_report['exit']=1;red.stdout=b'actual partial prefix\n';red.stderr=b'actual source failure\n'
        red.capture_report.update(stdout_bytes=len(red.stdout),stderr_bytes=len(red.stderr))
        actual=payloads(red);verify_retained(red,actual)
        corrupted=[]
        for name in actual:
            item=actual.copy();del item[name];corrupted.append(item)
            item=actual.copy();item[name]=actual[name][:-2] if name=='native.capture.json' else actual[name][:-1];corrupted.append(item)
            item=actual.copy();item[name]=b'';corrupted.append(item)
        for key,value in [('exit',0),('exit',True),('exit',1.0),('reader_errors',None),('extra',1)]:
            item=actual.copy();changed=copy.deepcopy(red.capture_report);changed[key]=value
            item['native.capture.json']=json.dumps(changed).encode();corrupted.append(item)
        for value in (b'{',b'null',b'[]',b'\xff',b'{"exit":1,"exit":1}',b'{"x":NaN}'):
            item=actual.copy();item['native.capture.json']=value;corrupted.append(item)
        for item in corrupted:
            try:verify_retained(red,item)
            except (ValueError,UnicodeError):retention+=1;continue
            raise ValueError('Corrupt returned red retention accepted')
    before={'world/level.dat':{'bytes':3,'sha256':'1'*64},'world/region/r.0.0.mcr':{'bytes':12288,'sha256':'2'*64},'world/preserve.bin':{'bytes':4,'sha256':'4'*64}}
    after=copy.deepcopy(before);after['world/session.lock']={'bytes':8,'sha256':'3'*64}
    literal_inputs(before,after,{'world/session.lock'},True);literal_inputs(before,before,{'world/session.lock'},False)
    bad=[]
    for name,value in [('world/level.dat',{'bytes':3,'sha256':'5'*64}),('world/session.lock',{'bytes':9,'sha256':'3'*64}),('world/session.lock',{'bytes':True,'sha256':'3'*64}),
        ('world/preserve.bin',{'bytes':4,'sha256':'bad'}),('world/unexpected',{'bytes':8,'sha256':'6'*64})]:
        item=copy.deepcopy(after);item[name]=value;bad.append(item)
    item=copy.deepcopy(after);del item['world/level.dat'];bad.append(item);bad.append(before)
    for item in bad:
        try:literal_inputs(before,item,{'world/session.lock'},True)
        except ValueError:literals+=1;continue
        raise ValueError('Corrupt literal input accepted')
    for data in (b'',b'{}',b'{"selection":"Tile1"}',b'{"selection":"Tile2"}'):
        try:bundle_payload(data)
        except ValueError:bundles+=1;continue
        raise ValueError('Unapproved bundle accepted')
    require((rejected,retention,literals,bundles)==(168,80,7,4),'Every declared pure corruption refused')
    roots = literal_root_self_test()
    print('PASS:',rejected,'kernel transport refusals;',retention,'retained-red payload refusals;',literals,'literal guards;',bundles,'dependency bundle refusals;',roots,'real literal-root refusals')


def pin(path):
    data=path.read_bytes()
    return {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--mode',choices=MODES)
    parser.add_argument('--executable',type=Path)
    parser.add_argument('--work-dir',type=Path)
    parser.add_argument('--bundle',type=Path)
    parser.add_argument('--project-root',type=Path)
    args=parser.parse_args()
    if args.self_test:self_test();return 0
    if args.mode is None or args.executable is None or args.work_dir is None or args.bundle is None:
        parser.error('mode, executable, work-dir and explicit dependency bundle required')
    args.work_dir.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='kernel-'+args.mode+'-',dir=args.work_dir.resolve()))
    report={'status':'RUNNING','mode':args.mode,'fixture_preparation_manifest':FIXTURE_MANIFEST,'scope':'Finite genuine native kernel control; external legacy graph contract; full foreign query/joint Store/SL OPEN'}
    before=literal_before=None
    root=executable=ownership=fixture_root=None
    launch_attempted=protocol_complete=False
    child=None

    def fail(key,error):
        detail={'type':type(error).__name__,'message':str(error)}
        report[key]=detail;report.setdefault('failure',detail);report['status']='FAIL'

    def load(name,relative):
        spec=importlib.util.spec_from_file_location(name,root/relative)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

    def guarded():
        result=ownership.protected(executable)
        for relative,expected in {**bundle['sources'],**bundle['helpers']}.items():
            path=(root/relative).resolve(strict=True)
            require(path.is_relative_to(root),'Bundle path outside actual source root')
            require(pin(path)==expected,'Selected source/helper mismatch: '+relative)
            result[str(path)]=expected
        for path in (Path(__file__).resolve(),args.bundle.resolve(strict=True)):
            result[str(path)]=pin(path)
        return result

    def files():
        return fixture_files(fixture_root)

    try:
        print(json.dumps({'retained_work':str(out),'stage':'PREPARING'}),flush=True)
        root=(args.project_root if args.project_root is not None else Path(__file__).resolve().parents[3]).resolve(strict=True)
        require(args.bundle.is_file() and not args.bundle.is_symlink() and args.bundle.stat().st_size<=LIMIT,'Bundle file type/extent')
        bundle=bundle_payload(args.bundle.read_bytes());report['dependency_bundle']=bundle;report['bundle_pin']=pin(args.bundle)
        for relative,expected in bundle['helpers'].items():require(pin(root/relative)==expected,'Frozen actual helper changed: '+relative)
        ownership=load('kernel_actual_ownership','tests/tools/world/test_execution_ownership.py')
        executable=args.executable.resolve(strict=True);require(executable.is_file(),'Executable is not a regular file')
        before=guarded();report['guard_before']=before
        require(len(ownership.TABLES)==6,'Expected six current provenance tables')
        report['tables']=list(ownership.TABLES)
        support=load('kernel_actual_capture','tests/reference/client/statistics/support.py')
        recipe=load('kernel_actual_recipe','tests/tools/world/test_source_saving.py')
        fixture_root=out/'owned';require(str(fixture_root).isascii(),'Existing actual source fixture requires ASCII root')
        (fixture_root/'world/region').mkdir(parents=True);(fixture_root/'snapshots').mkdir()
        info,image=recipe.seed('empty' if args.mode=='empty' else 'item-chest')
        (fixture_root/'world/level.dat').write_bytes(info);(fixture_root/'world/region/r.0.0.mcr').write_bytes(image)
        (fixture_root/'world/preserve.bin').write_bytes(b'unrelated disposable World file\0\xff')
        literal_before=files();report['literal_before']=literal_before
        command=[str(executable),args.mode,str(fixture_root)]
        report['command']=command;launch_attempted=True
        child=support.run_bounded(command,out,'native',timeout=TIMEOUT,limit=LIMIT,env=os.environ.copy())
        report['capture']=child.capture_report;report['checks']=completed(child,args.mode,command)
        protocol_complete=True;report['status']='PASS'
    except BaseException as error:
        fail('initial_failure',error)
        if getattr(error,'capture_report',None) is not None:report['failed_capture']=error.capture_report
    finally:
        report.update(native_launch_attempted=launch_attempted,native_child_returned=child is not None,native_protocol_complete=protocol_complete)
        report['guards_unchanged']=False
        if before is not None:
            try:
                after=guarded();report['guard_after']=after
                require(after==before,'Source/product/helper/table/bundle set changed')
                report['guards_unchanged']=True
            except BaseException as error:fail('terminal_guard_failure',error)
        else:report['status']='FAIL'
        if literal_before is not None:
            try:
                terminal_literal_guard(report, fixture_root, literal_before, protocol_complete)
            except BaseException as error:fail('literal_guard_failure',error)
        elif fixture_root is not None:
            # No before snapshot means preparation already failed. Preserve
            # any partial files without turning that setup failure into PASS.
            report['literal_snapshot_not_established'] = True
            try:
                if fixture_root.exists():
                    report['literal_partial_after'] = files()
                else:
                    report['literal_partial_after'] = {'root_missing': True}
            except BaseException as error:fail('literal_partial_guard_failure',error)
        byte_map={};capture_files={}
        for leaf in ('native.stdout','native.stderr','native.capture.json'):
            path=out/leaf
            try:
                if not path.exists():capture_files[leaf]={'missing':True};continue
                require(path.is_file() and not path.is_symlink() and path.stat().st_size<=LIMIT,'Retained file type/extent: '+leaf)
                data=path.read_bytes();require(pin(path)=={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()},'Capture changed during terminal read')
                byte_map[leaf]=data;capture_files[leaf]=pin(path)
            except BaseException as error:
                capture_files[leaf]={'failure':{'type':type(error).__name__,'message':str(error)}}
                if child is not None:fail('retention_read_failure',error)
        report['capture_files']=capture_files
        if child is not None:
            try:verify_retained(child,byte_map);report['retained_capture_verified']=True
            except BaseException as error:fail('retention_failure',error);report['retained_capture_verified']=False
        else:
            report['retained_capture_verified']=None
            report['retention_scope']='No returned child; partial files retained/pinned, original preflight/spawn/capture failure remains FAIL'
        with (out/'report.json').open('x',encoding='utf-8',newline='\n') as stream:stream.write(json.dumps(report,indent=2)+'\n')
    if report['status']!='PASS':
        print('FAIL: actual kernel control; retained',out/'report.json',file=sys.stderr);return 1
    print('PASS:',report['checks'],'actual kernel checks;mode='+args.mode+';retained',out);return 0


if __name__=='__main__':
    raise SystemExit(main())
