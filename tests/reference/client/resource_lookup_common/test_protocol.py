"""Normal/-O source controls; synthetic records never stand in for observations."""
import argparse
import copy
import json
from pathlib import Path
import sys
import unittest
sys.dont_write_bytecode = True
from protocol import LIMIT, PREFIX, completed, compare, need, parse, validate
from bootstrap_origin import admit_with_metadata


def parser_fixture():
    # Arbitrary authored parser data, not an expected/captured game answer.
    events = []
    labels = ['water','cache','grass','foliage','missing','clamp','blur']
    ids = [1,1,2,3,4,5,6]
    for label, identity in zip(labels, ids):
        events.append({'label':label, 'outcome':'return', 'exception':None,
            'array':{'id':identity,'length':1,'hex':'a1b2c3d4'},
            'state':{'b':[],'c':[],'d_count':0,'j':False,'k':False,'ledger':[],
                     'e':[0,1,1],'e0':0,'f':[0,1048576,1048576],'l_null':False,'selected_null':False}})
    return {'schema':1,'runtime':'original','mode':'color','profile':'default',
        'setup':{'actual_app':True,'app_z_alias':True,'pack':'pa','engine_i_null':True},
        'device':None,'events':events}


def encode(value, banner=b'authored parser prefix\r\n'):
    return banner + PREFIX + json.dumps(value,separators=(',',':')).encode() + b'\n'


class ProtocolControls(unittest.TestCase):
    def test_transport_and_prefix(self):
        value = parser_fixture()
        actual, raw = parse(encode(value))
        self.assertEqual(raw, b'authored parser prefix\r\n')
        validate(actual,'color','default','original')
        for suffix in (b'', b'\r'):
            actual, raw = parse(encode(value).rstrip(b'\n') + suffix + b'\n')
            validate(actual,'color','default','original')
            self.assertEqual(raw, b'authored parser prefix\r\n')

    def test_malformed_transport(self):
        raw = encode(parser_fixture())
        bad = [b'', raw[:-1], raw + b'x\n', raw + raw, raw.replace(b'"schema":1',b'"schema":1,"schema":1'),
               raw.replace(b'"schema":1',b'"schema":NaN'), b'x'*(LIMIT+1), raw.replace(b'LOOKUP_RECORD ',b'LOOKUP_RECORD \x00')]
        for data in bad:
            with self.subTest(bytes=len(data)), self.assertRaises((ValueError,UnicodeError)):
                parse(data)

    def test_structural_tamper(self):
        mutators = [lambda x:x.update(schema=True), lambda x:x.update(runtime='native'),
            lambda x:x.update(profile='zip'), lambda x:x['setup'].update(pack='vp'),
            lambda x:x['setup'].update(actual_app=False), lambda x:x.update(device={}),
            lambda x:x['events'].pop(), lambda x:x['events'].reverse(),
            lambda x:x['events'][0].update(label='forged'),
            lambda x:x['events'][0]['array'].update(length=2),
            lambda x:x['events'][0]['array'].update(hex='0'),
            lambda x:x['events'][0]['array'].update(id=0),
            lambda x:x['events'][1]['array'].update(id=2),
            lambda x:x['events'][0]['state'].update(e=[2,1,1]),
            lambda x:x['events'][0]['state'].update(ledger=[1]),
            lambda x:x['events'][0].update(outcome='frontier'),
            lambda x:x['events'][0].update(exception='java.lang.Error'),
            lambda x:x['events'][0].update(extra=True)]
        for mutate in mutators:
            value = parser_fixture(); mutate(value)
            with self.subTest(mutation=mutators.index(mutate)), self.assertRaises(ValueError):
                validate(value,'color','default','original')

    def test_actual_first_use_stream_order(self):
        from origin import colorizer_activation
        def data(profile, warm=False):
            rows = []
            role = 'static' if profile == 'statics' else 'setter'
            for owner in ('hw','ia','jh'):
                event = "[0.1s][info][class,init] 1 Initializing '" + owner + "'\n"
                before = 'LOOKUP_BOUNDARY ' + role + '-before ' + owner + '\n'
                after = 'LOOKUP_BOUNDARY ' + role + '-after ' + owner + '\n'
                rows.extend([event,before,after] if warm else [before,event,after])
            return ''.join(rows)
        for profile in ('statics','first-setter'):
            raw = data(profile)
            self.assertTrue(colorizer_activation(raw,profile)['all_observed_uses_are_first'])
            self.assertFalse(colorizer_activation(data(profile,True),profile)['all_observed_uses_are_first'])
            self.assertTrue(all(row['state'] == 'already_initialized_before_observed_use' for row in colorizer_activation(data(profile,True),profile)['classes'].values()))
            for damaged in [raw.replace("Initializing 'hw'", "Initializing 'other'"),
                            raw + "[0.2s][info][class,init] 2 Initializing 'hw'\n",
                            raw.replace('LOOKUP_BOUNDARY', 'MISSING_BOUNDARY', 1)]:
                with self.assertRaises(ValueError): colorizer_activation(damaged,profile)

    def test_unified_library_tag_transport(self):
        from origin import library_load_names, source_diagnostics
        # Authored parser strings only: no JDK/JAR/log fixture or native load.
        message = ' Loaded library declared-name.dll, handle 0x1234\n'
        for tag in ('library', 'library   '):
            raw = ('[0.123s][info][' + tag + ']' + message).encode('ascii')
            self.assertEqual(source_diagnostics(raw, 'default'), b'')
            self.assertEqual(library_load_names(raw.decode()), ['declared-name.dll'])
        for tag in ('library ', 'library  ', 'library    ', 'library\t', 'library-extra'):
            raw = ('[0.123s][info][' + tag + ']' + message).encode('ascii')
            self.assertEqual(library_load_names(raw.decode()), [])
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                source_diagnostics(raw, 'default')
        diagnostic = b'authored real diagnostic\r\n'
        log = b'[0.123s][info][library   ] Loaded library raw-name.dll, handle 0x1234\n'
        with self.assertRaises(ValueError): source_diagnostics(log + diagnostic, 'default')
        self.assertEqual(source_diagnostics(log + diagnostic, 'frontiers'), diagnostic)
        self.assertEqual(library_load_names('[0.123s][info][library] Loaded library jsvml.dll, handle 0x1234\n'), ['jsvml.dll'])

    def test_process_completion(self):
        executable = Path(__file__).resolve()
        report = {'command':[str(executable)],'exit':0,'reaped':True,'pipe_capture_complete':True,
            'reader_errors':{},'timed_out':False,'output_overflow':False,'stdout_bytes':1,
            'stderr_bytes':0,'timeout_seconds':180,'limit':LIMIT}
        completed(report,b'x',b'',executable)
        for key,value in [('exit',77),('exit',True),('reaped',False),('pipe_capture_complete',False),
            ('reader_errors',{'stdout':['read']}),('timed_out',True),('output_overflow',True),
            ('stdout_bytes',0),('stdout_bytes',1.0),('stderr_bytes',1),('stderr_bytes',False),('timeout_seconds',181),('timeout_seconds',180.0),('limit',LIMIT+1),('limit',float(LIMIT)),('command',[]),('command',[str(executable),False])]:
            damaged = dict(report); damaged[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): completed(damaged,b'x',b'',executable)

    def test_native_IDs_not_rewritten(self):
        original = parser_fixture(); native = copy.deepcopy(original); native['runtime'] = 'native'
        from test_admission import native_diagnostic
        for event in native['events']: event['native'] = native_diagnostic()
        self.assertEqual(compare(original,native)['status'],'MATCH')
        native['events'][0]['array']['hex'] = '01020304'
        self.assertEqual(compare(original,native)['status'],'MISMATCH')

    def test_exact_bootstrap_nest(self):
        digest = {'sha256':'0'*64}
        classes = {'jdk.Host':{'NestHost':None,'NestMembers':['jdk.Host$Nested'],
            'JMOD_class_identical':True,'module':'java.base','actual_modules_class':digest},
            'jdk.Host$Nested':{'NestHost':'jdk.Host','NestMembers':[],
            'JMOD_class_identical':True,'module':'java.base','actual_modules_class':digest}}
        previous = {'jdk.Host':'jrt:/java.base','jdk.Host$Nested':'jrt:/java.base'}
        name = 'jdk.Host$Nested$$Lambda$1/0x1234'
        admit_with_metadata(name,'jdk.Host',previous,set(),classes)
        for host,seen,forbidden,proof in [('jdk',previous,set(),classes),
                ('jdk.Host',{},set(),classes),('jdk.Host',previous,{'jdk.Host'},classes),
                ('jdk.Host',previous,set(),{'jdk.Host':classes['jdk.Host']})]:
            with self.assertRaises(ValueError): admit_with_metadata(name,host,seen,forbidden,proof)


def actual_replay(directory):
    # Replay is an explicit later read-only gate; it never starts a native/JVM child.
    from capture import product_map, source_inventory
    from protocol import fingerprint
    report = json.loads((directory / 'REPORT.json').read_text())
    from capture import ACCEPTED
    need(report['status'] in ACCEPTED, 'accepted closed actual report; read-only consistency replay is not an operational trust anchor')
    receipt = report['native']; capture = receipt['capture']; executable = Path(capture['command'][0])
    stdout, stderr = (directory/'native.stdout').read_bytes(), (directory/'native.stderr').read_bytes()
    completed(capture,stdout,stderr,executable)
    need(fingerprint(directory/'native.stdout') == receipt['stdout'] and fingerprint(directory/'native.stderr') == receipt['stderr'], 'exact captured actual native streams')
    manifest = receipt['artifact_manifest']
    need(fingerprint(Path(manifest['path'])) == manifest['pin'], 'unchanged exact artifact-map receipt')
    need(product_map(Path(manifest['path']),executable,manifest['pin']['sha256']) == receipt['actual_product_map'], 'actual retained native product identity')
    if 'product_gate' in report:
        gate = report['product_gate']
        need(fingerprint(Path(gate['path'])) == gate['pin'] and
             json.loads(Path(gate['path']).read_text()) == gate['record'], 'exact independent closed qualification gate')
    actual, prefix = parse(stdout); validate(actual,report['mode'],report['profile'],'native')
    need(prefix == (directory/'native-source-stdout.raw').read_bytes(), 'byte-exact source banner replay')
    need(fingerprint(directory/'native-source-stdout.raw') == report['native_source_banners'], 'exact source-banner pin')
    metadata = {'source_files':report['source_before']}
    need(source_inventory(Path(report['source_root']),metadata) == report['source_before'], 'actual source/helper/JAR-authority-policy closure')
    damaged = copy.deepcopy(metadata)
    relative = next(iter(damaged['source_files']))
    damaged['source_files'][relative]['sha256'] = '0'*64
    try: source_inventory(Path(report['source_root']),damaged)
    except ValueError: pass
    else: raise ValueError('actual source-leaf tamper admitted')
    try: product_map(Path(manifest['path']),executable,'0'*64)
    except ValueError: pass
    else: raise ValueError('actual qualified artifact-map tamper admitted')
    if 'original' in report:
        original = report['original']
        out, err = (directory/'original.stdout').read_bytes(), (directory/'original.stderr').read_bytes()
        completed(original['capture'],out,err,Path(original['capture']['command'][0]))
        need(fingerprint(directory/'original.stdout') == original['stdout'] and fingerprint(directory/'original.stderr') == original['stderr'], 'exact independent original streams')
        before, banners = parse(out); validate(before,report['mode'],report['profile'],'original')
        need(banners == (directory/'original-source-stdout.raw').read_bytes() and
             fingerprint(directory/'original-source-stdout.raw') == original['source_banners'], 'exact raw original source banners')
        need(compare(before,actual) == report['comparison'], 'replay actual complete comparison without rewriting receipts')
    for key in ('exit','reaped','pipe_capture_complete','reader_errors','timed_out','output_overflow'):
        modified = dict(capture)
        modified[key] = 77 if key == 'exit' else {'forged':['reader']} if key == 'reader_errors' else not capture[key]
        try: completed(modified,stdout,stderr,executable)
        except ValueError: pass
        else: raise ValueError('actual receipt tamper admitted: ' + key)
    return {'scope':'actual read-only receipt/stream tamper replay, no extra game call','mode':report['mode'],'profile':report['profile']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--run-dir',type=Path)
    args, remaining = parser.parse_known_args()
    if args.run_dir is not None: print(json.dumps(actual_replay(args.run_dir.resolve(strict=True))))
    unittest.main(argv=[sys.argv[0],*remaining])
