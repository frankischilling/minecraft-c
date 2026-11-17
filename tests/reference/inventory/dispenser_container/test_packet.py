"""Portable synthetic Dispenser corruption controls; no target execution."""
import copy
import importlib.util
import json
import tempfile
import io
import contextlib
import zipfile
import hashlib
import sys
import shutil
import types
sys.dont_write_bytecode = True
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


A = load('dispenser_controls_audit', HERE / 'audit.py')
C = R = A.M
def synthetic_row(case, ordinal):
    failure = R.expected_failure(case)
    graph = None
    if failure is not None:
        frames = [{'class': c, 'method': m, 'file': 'SourceFile', 'line': -1, 'native': False}
                  for c, m in failure[1]]
        frames += [{'class': 'DispenserContainerObservation', 'method': m,
                    'file': None, 'line': -1, 'native': False} for m in ('observe', 'main')]
        graph = {'root': 0, 'nodes': [{'id': 0, 'class': failure[0], 'message': 'synthetic raw message',
                 'cause': None, 'suppressed': [], 'frames': frames}]}
    snapshot = R.expected_snapshot(case)
    return dict(case, ordinal=ordinal, layout=R.expected_layout(case), base={'window': 0, 'transaction': 0,
        'slots': 45, 'snapshots': 45, 'snapshots_null': True, 'listeners': 0, 'blocked': 0,
        'collections_distinct': True, 'captured_az_same': True}, before=snapshot,
        after=copy.deepcopy(snapshot), returned=failure is None,
        result={'get-az': 'A', 'get-ix-main': 'B', 'get-ix-hotbar': 'H'}.get(case['case']), throwable=graph)


def detail(original):
    op, name = original['operation'], original['case']
    failure = original['throwable'] is not None
    status = (3 if name in ('get-negative', 'get-end') else 2) if failure else 0
    call = op in ('getter', 'click')
    exception = original['throwable']['nodes'][0]['class'].replace('.', '/') if failure else None
    inventory = C.empty_inventory()
    if op == 'usable':
        inventory.update(status=status, source_exception=True, exception_class=exception,
                         operation='synthetic actual operation', frames=[C.site('sh', 'b', '(Lgs;)Z', 5)])
        if name == 'usable-null-world':
            inventory['frames'].append(C.site('az', 'a_', '(Lgs;)Z', 16))
    target_site = child = transfer = copy.deepcopy(C.NONE_SITE)
    if op == 'getter':
        pc = 5 if status == 3 else 19 if failure else 22
        target_site = transfer = C.site('dw', 'a', '(I)Liz;', pc)
        child = C.site('dw', 'a', '(I)Liz;', 5) if status == 3 else C.site('gp', 'a', '()Liz;', 8 if failure else 13)
    elif op == 'click':
        target_site = C.site('dw', 'a', '(IIZLgs;)Liz;', 3)
    value = {'ordinal': original['ordinal'], 'case': name, 'status': status, 'has_call': call,
        'usable_sentinel': True, 'inventory': inventory, 'interaction': {'status': status if call else 0,
        'operation': 'synthetic actual operation' if call and failure else None,
        'exception_class': exception if call else None, 'native_policy': not (call and failure),
        'source_exception': call and failure, 'source_error': False, 'site': target_site, 'child': child,
        'dependency': copy.deepcopy(C.NONE_SITE), 'transfer': transfer, 'inventory': C.empty_inventory(),
        'registry': C.empty_registry(-1), 'stack': 0}, 'call_state': {'returned': call and not failure,
        'active': False, 'source_pc': target_site['pc'] if call else 0, 'shift_calls': 1 if op == 'getter' else 0,
        'drop_calls': 0, 'steps': 1 if op == 'click' else 0, 'cursor_cleared': False},
        'constructor_pc': 173, 'inventory_calls': 0, 'cursor_calls': 0}
    for inventory in (value['inventory'], value['interaction']['inventory']):
        inventory.update(dispenser_diagnostic_owner_id_is_zero=True, exception_class_from_dispenser=False)
    value['interaction']['exception_class_dispenser_owner_id_is_zero'] = True
    value['adapter_diagnostic_owner_present'] = original['az_present']
    value['adapter_diagnostic_owner_stable'] = original['az_present']
    return value


def baseline_controls():
    results = []
    def positive(name, action):
        action()
        results.append({'name': name, 'kind': 'positive', 'status': 'PASS'})
    def negative(name, action):
        try:
            action()
        except (ValueError, KeyError, TypeError, UnicodeError):
            results.append({'name': name, 'kind': 'negative', 'status': 'PASS'})
        else:
            raise ValueError('Accepted negative: ' + name)
    cases = R.declarations((HERE / 'cases.tsv').read_bytes())
    rows = [synthetic_row(case, index) for index, case in enumerate(cases)]
    included = [row for row in rows if row['case'] not in R.ORIGINAL_ONLY]
    details = [detail(row) for row in included]
    for index, value in enumerate(details):
        positive('complete synthetic diagnostic ' + value['case'],
                 lambda i=index, v=value: C.prior_detail(v, included[i]))
    stdout = b'\n'.join(C.PREFIX) + b'\n' + ('\n'.join('SH_NATIVE_ROW ' + json.dumps(R.common_projection(row))
             for row in included) + '\nSH_NATIVE_COMPLETE 16\n').encode()
    def diagnostic_bytes(values):
        result = []
        for v in values:
            result += ['SH_NATIVE_DETAIL ' + json.dumps(v), 'SH_NATIVE_RETIRE ' + json.dumps(
                       {'ordinal': v['ordinal'], 'case': v['case'], 'retired': True})]
        return ('\n'.join(result) + '\nSH_NATIVE_DETAIL_COMPLETE 16\n').encode()
    stderr = diagnostic_bytes(details)
    positive('complete synthetic component join', lambda: C.join(rows, stdout, stderr))
    def corrupt(index, change):
        values = copy.deepcopy(details)
        change(values[index])
        C.join(rows, stdout, diagnostic_bytes(values))
    mutations = [
        (0, 'unknown detail key', lambda v: v.update(extra=1)),
        (0, 'V2 nonzero inventory owner', lambda v: v['inventory'].update(dispenser_diagnostic_owner_id_is_zero=False)),
        (12, 'V2 static NPE marked receipt-derived', lambda v: v['inventory'].update(exception_class_from_dispenser=True)),
        (4, 'V2 nonzero nested inventory owner', lambda v: v['interaction']['inventory'].update(dispenser_diagnostic_owner_id_is_zero=False)),
        (10, 'V2 static GP NPE marked receipt-derived', lambda v: v['interaction']['inventory'].update(exception_class_from_dispenser=True)),
        (8, 'V2 bounds name given adapter owner', lambda v: v['interaction'].update(exception_class_dispenser_owner_id_is_zero=False)),
        (4, 'V2 present adapter missing witness', lambda v: v.update(adapter_diagnostic_owner_present=False)),
        (4, 'V2 actual adapter identity changed', lambda v: v.update(adapter_diagnostic_owner_stable=False)),
        (0, 'V2 null adapter fabricated witness', lambda v: v.update(adapter_diagnostic_owner_present=True)),
        (0, 'V2 null adapter fabricated identity credit', lambda v: v.update(adapter_diagnostic_owner_stable=True)),
        (4, 'V2 integer witness boolean', lambda v: v['inventory'].update(dispenser_diagnostic_owner_id_is_zero=1)),
        (4, 'V2 missing inventory witness', lambda v: v['inventory'].pop('dispenser_diagnostic_owner_id_is_zero')),
        (4, 'V2 missing name-origin flag', lambda v: v['inventory'].pop('exception_class_from_dispenser')),
        (4, 'V2 missing interaction witness', lambda v: v['interaction'].pop('exception_class_dispenser_owner_id_is_zero')),
        (4, 'V2 missing adapter stability', lambda v: v.pop('adapter_diagnostic_owner_stable')),
        (0, 'boolean ordinal', lambda v: v.update(ordinal=False)),
        (0, 'wrong ctor PC', lambda v: v.update(constructor_pc=174)),
        (0, 'unknown inventory key', lambda v: v['inventory'].update(extra=1)),
        (0, 'retained opaque World child', lambda v: v['inventory'].update(dispenser_present=True)),
        (0, 'Stack cause', lambda v: v['inventory'].update(stack_status=1)),
        (0, 'NBT cause', lambda v: v['inventory']['stack']['nbt'].update(offset=1)),
        (0, 'Registry cause', lambda v: v['inventory']['registry'].update(status=1)),
        (0, 'format cause', lambda v: v['inventory']['registry']['format_error'].update(flags=1)),
        (0, 'dirty mutation', lambda v: v['inventory'].update(mutation_reached=True)),
        (0, 'dirty notification', lambda v: v['inventory'].update(notification_reached=True)),
        (0, 'cursor host callback', lambda v: v.update(cursor_calls=1)),
        (0, 'inventory host callback', lambda v: v.update(inventory_calls=1)),
        (4, 'getter status failure', lambda v: v.update(status=2)),
        (4, 'getter extra clone step', lambda v: v['call_state'].update(steps=1)),
        (4, 'getter recursive transfer', lambda v: v['call_state'].update(shift_calls=2)),
        (4, 'getter nonreturn', lambda v: v['call_state'].update(returned=False)),
        (4, 'getter active call', lambda v: v['call_state'].update(active=True)),
        (4, 'getter wrong child frame', lambda v: v['interaction']['child'].update(pc=8)),
        (8, 'bounds wrong exception class', lambda v: v['interaction'].update(exception_class='java/lang/ArrayIndexOutOfBoundsException')),
        (8, 'bounds marked native policy', lambda v: v['interaction'].update(native_policy=True)),
        (8, 'bounds wrong Source PC', lambda v: v['interaction']['site'].update(pc=19)),
        (10, 'null LW missed GP frame', lambda v: v['interaction']['child'].update(**{'class': 'dw'})),
        (12, 'null AZ invented AZ frame', lambda v: v['inventory']['frames'].append(C.site('az', 'a_', '(Lgs;)Z', 16))),
        (13, 'null World dropped AZ frame', lambda v: v['inventory']['frames'].pop()),
        (13, 'failure changed output sentinel', lambda v: v.update(usable_sentinel=False)),
        (14, 'invalid button reaches GS', lambda v: v['call_state'].update(source_pc=14)),
        (14, 'invalid button shift transfer', lambda v: v['call_state'].update(shift_calls=1)),
    ]
    for index, name, change in mutations:
        negative(name, lambda i=index, f=change: corrupt(i, f))
    v1_details = [C.prior_detail(value, original) for value, original in zip(copy.deepcopy(details), included)]
    negative('V2 refuses historical V1 diagnostic schema', lambda: C.join(rows, stdout, diagnostic_bytes(v1_details)))
    def exact_witness_scope():
        summary = C.join(rows, stdout, stderr)['typed_witness_observations']
        R.equal(summary['present_stable_adapter_witnesses'], 11, 'Eleven genuine nonnull adapter cases')
        R.equal(summary['inventory_errors_with_receipt_derived_name'], 0, 'No synthetic producer qualification')
        R.equal(summary['retained_WorldAccess_name_expiry_qualified'], False, 'No expiry behavior qualification')
        R.equal(summary['absent_adapters_receive_identity_credit'], False, 'No null object identity credit')
    positive('V2 exact bounded native witness scope', exact_witness_scope)
    negative('bootstrap missing', lambda: C.join(rows, stdout.split(b'\n', 2)[2], stderr))
    negative('bootstrap swapped', lambda: C.join(rows, stdout.replace(b'151 recipes\n16 achievements', b'16 achievements\n151 recipes'), stderr))
    negative('unknown stdout preamble', lambda: C.join(rows, b'ignored\n' + stdout, stderr))
    negative('projection clone substitution', lambda: C.join(rows, stdout.replace(b'"result": "A"', b'"result": "OTHER"', 1), stderr))
    negative('projection mutation', lambda: C.join(rows, stdout.replace(b'"changed": false', b'"changed": true', 1), stderr))
    negative('projection summary only', lambda: C.join(rows, b'151 recipes\n16 achievements\nSH_NATIVE_COMPLETE 16\n', stderr))
    negative('missing stdout terminal LF', lambda: C.join(rows, stdout[:-1], stderr))
    negative('unknown stderr', lambda: C.join(rows, stdout, stderr + b'ignored\n'))
    negative('missing diagnostics terminal', lambda: C.join(rows, stdout, stderr[:stderr.rfind(b'SH_NATIVE_DETAIL_COMPLETE')]))
    negative('missing retirement', lambda: C.join(rows, stdout, b'\n'.join(stderr.splitlines()[1:]) + b'\n'))
    negative('failed retirement', lambda: C.join(rows, stdout, stderr.replace(b'"retired": true', b'"retired": false', 1)))
    negative('duplicated diagnostic key', lambda: C.join(rows, stdout, stderr.replace(b'"has_call": false', b'"has_call": false,"has_call": false', 1)))
    print(json.dumps({'status': 'PASS_SYNTHETIC_OFFLINE_ONLY', 'controls': len(results),
          'positive': sum(v['kind'] == 'positive' for v in results),
          'negative': sum(v['kind'] == 'negative' for v in results), 'results': results,
          'native_executions': 0, 'compiler_executions': 0, 'JVM_executions': 0}, indent=2))



def original_baseline_controls():
    results = []
    def positive(name, action):
        action()
        results.append({'name': name, 'kind': 'positive', 'status': 'PASS'})
    def negative(name, action):
        try:
            action()
        except (ValueError, KeyError, UnicodeError, TypeError):
            results.append({'name': name, 'kind': 'negative', 'status': 'PASS'})
        else:
            raise ValueError('Accepted negative control: ' + name)
    declared = (HERE / 'cases.tsv').read_bytes()
    cases = R.declarations(declared)
    rows = [synthetic_row(case, index) for index, case in enumerate(cases)]
    for index, item in enumerate(rows):
        positive('complete synthetic case ' + item['case'], lambda i=index, v=item: R.validate_row(v, cases[i], i))
    def corrupt(index, change):
        item = copy.deepcopy(rows[index])
        change(item)
        R.validate_row(item, cases[index], index)
    mutations = [
        (0, 'extra row key', lambda r: r.update(extra=0)),
        (0, 'missing row key', lambda r: r.pop('base')),
        (0, 'boolean ordinal', lambda r: r.update(ordinal=False)),
        (0, 'wrong case', lambda r: r.update(case='layout-01')),
        (0, 'wrong nullable identity', lambda r: r['layout'][0].update(owner='AZ')),
        (3, 'wrong player alias', lambda r: r['layout'][9].update(owner='AZ')),
        (3, 'wrong slot count', lambda r: r['layout'].pop()),
        (3, 'wrong slot coordinate', lambda r: r['layout'][44].update(y=141)),
        (3, 'wrong slot index', lambda r: r['layout'][36].update(inventory_index=36)),
        (3, 'specialized slot fabricated', lambda r: r['layout'][1].update(**{'class': 'ScriptedSlot'})),
        (3, 'unconstructed random', lambda r: r['before']['tile'].update(random_constructed=False)),
        (3, 'random substitute', lambda r: r['before']['tile'].update(random_class='ScriptedRandom')),
        (3, 'nonempty ctor tile', lambda r: r['before']['tile']['slots'].__setitem__(0, 'A')),
        (3, 'changed constructor flag', lambda r: r['before']['player'].update(changed=True)),
        (4, 'getter clone substituted', lambda r: r.update(result='N3')),
        (4, 'mutation after getter', lambda r: r['after']['stacks']['A'].__setitem__(1, 6)),
        (4, 'truncated armor', lambda r: r['before']['player']['armor'].pop()),
        (4, 'introduced cursor', lambda r: r['before']['player'].update(cursor='A')),
        (4, 'false return', lambda r: r.update(returned=False)),
        (4, 'boolean result', lambda r: r.update(result=True)),
        (5, 'empty getter invented alias', lambda r: r.update(result='A')),
        (8, 'wrong Throwable class', lambda r: r['throwable']['nodes'][0].update(**{'class': 'java.lang.ArrayIndexOutOfBoundsException'})),
        (8, 'dropped Throwable stack', lambda r: r['throwable']['nodes'][0].update(frames=[])),
        (10, 'extra Source frame', lambda r: r['throwable']['nodes'][0]['frames'].insert(0, {'class': 'sh', 'method': 'a', 'file': None, 'line': -1, 'native': False})),
        (10, 'exception cause', lambda r: r['throwable']['nodes'][0].update(cause=0)),
        (10, 'exception suppression', lambda r: r['throwable']['nodes'][0].update(suppressed=[0])),
        (10, 'exception graph truncated', lambda r: r['throwable']['nodes'].clear()),
        (12, 'null AZ invented callee', lambda r: r['throwable']['nodes'][0]['frames'][0].update(**{'class': 'az', 'method': 'a_'})),
        (13, 'null World dropped SH caller', lambda r: r['throwable']['nodes'][0]['frames'].pop(1)),
        (14, 'shift incorrectly reaches getter', lambda r: r.update(result='A')),
        (16, 'invalid button reads null GS', lambda r: r.update(returned=False)),
        (17, 'base lists alias', lambda r: r['base'].update(collections_distinct=False)),
    ]
    for index, name, change in mutations:
        negative(name, lambda i=index, c=change: corrupt(i, c))
    negative('duplicate JSON keys', lambda: R.parse('{"x":1,"x":1}'))
    negative('nonfinite JSON', lambda: R.parse('{"x":NaN}'))
    negative('truncated declaration', lambda: R.declarations(declared[:-1]))
    negative('CRLF declaration', lambda: R.declarations(declared.replace(b'\n', b'\r\n')))
    negative('missing case', lambda: R.declarations(b'\n'.join(declared.splitlines()[:-1]) + b'\n'))
    negative('no original-to-native null GS credit', lambda: R.common_projection(rows[14]))
    native = [R.common_projection(item) for item in rows if item['case'] not in R.ORIGINAL_ONLY]
    payload = ('\n'.join('SH_NATIVE_ROW ' + json.dumps(item) for item in native) + '\nSH_NATIVE_COMPLETE 16\n').encode()
    positive('exact synthetic 16-case native projection', lambda: R.projection_join(rows, payload))
    negative('native summary only', lambda: R.projection_join(rows, b'SH_NATIVE_COMPLETE 16\n'))
    negative('native clone corruption', lambda: R.projection_join(rows, payload.replace(b'"result": "A"', b'"result": "N3"', 1)))
    negative('native terminal count corruption', lambda: R.projection_join(rows, payload.replace(b'COMPLETE 16', b'COMPLETE 18')))
    negative('native extra bytes', lambda: R.projection_join(rows, payload + b'ignored\n'))
    with tempfile.TemporaryDirectory(prefix='sh-reader-synthetic-') as directory:
        jar = Path(directory) / 'fake-not-original.jar'
        jar.write_bytes(b'explicit synthetic fixture, never JVM input')
        pins = {'jar': R.pin(jar), 'classes': {name: {'class_bytes': 1, 'class_sha256': '0' * 64} for name in R.CORE}}
        origin = {'jar': str(jar.resolve()), 'jar_sha256': pins['jar']['sha256'],
                  'classes': {name: {'bytes': 1, 'sha256': '0' * 64} for name in R.CORE}}
        guard = {'network': 0, 'native_links': 0, 'execs': 0, 'writes': 0,
                 'deletes': 0, 'replacements': 0, 'installed': True}
        import hashlib
        complete = {'cases': 18, 'declaration_sha256': hashlib.sha256(declared).hexdigest(), 'scope': R.SCOPE}
        protocol = ('SH_ORIGINS ' + json.dumps(origin) + '\n' +
            '\n'.join('SH_ROW ' + json.dumps(item) for item in rows) + '\nSH_GUARD ' + json.dumps(guard) +
            '\nSH_COMPLETE ' + json.dumps(complete) + '\n').encode()
        positive('complete synthetic raw protocol', lambda: R.read_rows(protocol, declared, pins, jar))
        negative('unexpected preamble', lambda: R.read_rows(b'16 achievements\n' + protocol, declared, pins, jar))
        negative('missing terminal', lambda: R.read_rows(protocol[:protocol.rfind(b'SH_COMPLETE ')], declared, pins, jar))
        negative('wrong class origin', lambda: R.read_rows(protocol.replace(b'"bytes": 1', b'"bytes": 2', 1), declared, pins, jar))
        negative('denied native link', lambda: R.read_rows(protocol.replace(b'"native_links": 0', b'"native_links": 1'), declared, pins, jar))
        negative('disabled guard', lambda: R.read_rows(protocol.replace(b'"installed": true', b'"installed": false'), declared, pins, jar))
        negative('truncated final LF', lambda: R.read_rows(protocol[:-1], declared, pins, jar))
    print(json.dumps({'status': 'PASS_SYNTHETIC_OFFLINE_ONLY', 'controls': len(results),
          'positive': sum(r['kind'] == 'positive' for r in results),
          'negative': sum(r['kind'] == 'negative' for r in results), 'results': results,
          'process_launches': 0, 'JVM_executions': 0}, indent=2))


def main():
    results = []
    for function in (original_baseline_controls, baseline_controls):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            function()
        results += json.loads(output.getvalue())['results']
    def positive(name, action):
        action()
        results.append({'name': name, 'kind': 'positive', 'status': 'PASS'})
    def negative(name, action):
        try:
            action()
        except (ValueError, KeyError, TypeError, UnicodeError, OSError, zipfile.BadZipFile):
            results.append({'name': name, 'kind': 'negative', 'status': 'PASS'})
        else:
            raise ValueError('Accepted packet negative: ' + name)
    positive('closed complete portable packet', A.audit)
    cap = load('dispenser_controls_capture', HERE / 'capture.py')
    pub = load('dispenser_controls_publish', HERE / 'publish.py')
    class FakeBackend:
        def capture(self, command, output, stem, timeout, environment, working_directory):
            self.received = environment
            return self.subprocess.Popen(command, env=environment)
    calls = []
    def factory(*args, **kwargs):
        calls.append(kwargs['env'])
        return object()
    def launch(environment, backend=None):
        backend = FakeBackend() if backend is None else backend
        handles = []
        cap.child_transport(backend, ['synthetic-no-program'], HERE, 'unused', 1,
                            environment, handles, factory=factory)
        R.need(len(handles) == 1 and backend.received is environment and calls[-1] is environment,
               'Exact actual child-transport environment object')
    positive('actual transport unset five options', lambda: launch({}))
    positive('actual transport explicit-empty five options', lambda: launch({name: '' for name in A.OPTIONS}))
    for name in A.OPTIONS:
        before = len(calls)
        negative('actual Popen boundary rejects ' + name, lambda n=name: launch({n: 'synthetic-injection'}))
        R.equal(len(calls), before, 'No fake compiler/VM launched under injected ' + name)
    environment = {}
    positive('first child same mutable environment', lambda: launch(environment))
    environment['JDK_JAVAC_OPTIONS'] = 'synthetic-late-injection'
    before = len(calls)
    negative('second child rejects late environment mutation', lambda: launch(environment))
    R.equal(len(calls), before, 'No second fake launch after late mutation')
    negative('different environment object refused', lambda: cap.guarded_spawn(factory, {}, [], (), {'env': {}}))
    for bad in ('Z:/synthetic/private', '/home/synthetic/private', '/Users/synthetic/private', '\\' * 2 + 'synthetic/share'):
        negative('private-data path refusal ' + str(len(results)), lambda b=bad: A.privacy({'message': b}))
    negative('nonfinite public data', lambda: R.parse('{"x":NaN}'))
    negative('duplicate public data key', lambda: R.parse('{"x":1,"x":2}'))
    data = A.load(HERE / 'finite.json')
    def corrupt_data(change):
        value = copy.deepcopy(data)
        change(value)
        A.validate_data(value, (HERE / 'cases.tsv').read_bytes())
    for name, change in [
        ('source Stack mutation', lambda d: d['original_rows'][4]['after']['stacks']['A'].__setitem__(1, 6)),
        ('Source Throwable missing child', lambda d: d['original_rows'][13]['throwable']['nodes'][0]['frames'].pop(0)),
        ('original case omitted', lambda d: d['original_rows'].pop()),
        ('native case omitted', lambda d: d['native_rows'].pop()),
        ('native details reordered', lambda d: d['native_details'].reverse()),
        ('public historical rebinding', lambda d: d['historical_lineage']['public_reproducer'].update(historical_capture_rebound_to_public_tools=True)),
        ('fifth historical option omitted', lambda d: d['historical_lineage']['original']['all_five_actual_options'].pop('JDK_JAVAC_OPTIONS')),
        ('invented JDI live-Code claim', lambda d: d['historical_lineage']['original'].update(runtime_entry_trace_or_JDI_live_Code_claim=True)),
        ('original shift case silently added to native', lambda d: d['native_rows'].append(d['native_rows'][0])),
    ]:
        negative('semantic ' + name, lambda c=change: corrupt_data(c))
    with tempfile.TemporaryDirectory(prefix='dispenser packet controls ') as tmp:
        root = Path(tmp)
        relocated = root / 'relocated packet with spaces'
        shutil.copytree(HERE, relocated)
        positive('relocated packet audit has no private import dependency', lambda: A.audit(relocated))
        shadow = root / 'model.py'
        shadow.write_text('raise RuntimeError("synthetic shadow must not import")\n')
        positive('file-bound audit ignores cwd shadow', lambda: load('dispenser_relocated', relocated / 'audit.py').audit())
        fake_original = root / 'synthetic original input entry'
        fake_original.mkdir()
        recorded = {str(relocated / name): R.pin(relocated / name) for name in A.FILES}
        fake_run = {'status': 'PASS_ORIGINAL_COMPONENT_ONLY', 'scope': R.SCOPE,
            'schema': 'dispenser_original_capture_v2', 'work': str(fake_original.resolve()),
            'inputs_before': recorded, 'inputs_after': recorded}
        positive('fresh verifier accepts exact byte-identical relocated package',
                 lambda: pub.public_package(fake_run))
        guardless = (relocated / 'capture.py').read_text()
        guardless = guardless.replace("guard_environment(kwargs['env'])", 'None  # synthetic guard omitted')
        guardless = guardless.replace('        guard_environment(environment)', '        None  # synthetic guard omitted')
        (relocated / 'capture.py').write_text(guardless, encoding='utf8', newline='\n')
        repaired = A.load(relocated / 'packet.json')
        repaired['files']['capture.py'] = R.pin(relocated / 'capture.py')
        A.write(relocated / 'packet.json', repaired)
        fake_run['inputs_before'] = {str(relocated / name): R.pin(relocated / name) for name in A.FILES}
        fake_run['inputs_after'] = fake_run['inputs_before']
        A.write(fake_original / 'run.json', fake_run)
        def reject_guardless_original_entry():
            try:
                pub.original(fake_original)
            except BaseException as error:
                if not isinstance(error, ValueError) or 'Captured public manifest equals this frozen verifier package' not in str(error):
                    raise RuntimeError('Focused original() control refused at an unrelated precondition') from error
                raise
        negative('original entry refuses guardless controller with repaired manifest/input hashes',
                 reject_guardless_original_entry)
        shutil.copyfile(HERE / 'capture.py', relocated / 'capture.py')
        shutil.copyfile(HERE / 'packet.json', relocated / 'packet.json')
        changed = relocated / 'finite.json'
        altered = A.load(changed)
        altered['original_rows'][4]['result'] = 'OTHER'
        A.write(changed, altered)
        manifest = A.load(relocated / 'packet.json')
        manifest['files']['finite.json'] = R.pin(changed)
        A.write(relocated / 'packet.json', manifest)
        negative('repaired manifest cannot replace historical data', lambda: A.audit(relocated))
        shutil.copyfile(HERE / 'finite.json', changed)
        shutil.copyfile(HERE / 'packet.json', relocated / 'packet.json')
        identity = A.load(relocated / 'source-pins.json')
        identity['classes']['az']['class_sha256'] = '0' * 64
        A.write(relocated / 'source-pins.json', identity)
        repaired = A.load(relocated / 'packet.json')
        repaired['files']['source-pins.json'] = R.pin(relocated / 'source-pins.json')
        A.write(relocated / 'packet.json', repaired)
        negative('repaired manifest cannot replace original Class identity', lambda: A.audit(relocated))
        shutil.copyfile(HERE / 'source-pins.json', relocated / 'source-pins.json')
        shutil.copyfile(HERE / 'packet.json', relocated / 'packet.json')
        bad_identity = A.load(HERE / 'source-pins.json')
        bad_identity['classes']['az']['methods'][0]['instructions'] = ['synthetic-prohibited-body']
        negative('original instruction body metadata refused', lambda: A.validate_pins(bad_identity))
        bad_identity = A.load(HERE / 'source-pins.json')
        bad_identity['jar']['sha256'] = '0' * 64
        negative('wrong original archive identity refused', lambda: A.validate_pins(bad_identity))
        (relocated / 'original.class').write_bytes(b'synthetic-only-not-a-class')
        negative('undeclared binary refused', lambda: A.audit(relocated))
        streams = root / 'streams'
        streams.mkdir()
        (streams / 'out').write_bytes(b'synthetic')
        (streams / 'err').write_bytes(b'')
        receipt = {'exit': 0, 'timed_out': False, 'output_overflow': False,
            'pipe_capture_complete': True, 'reader_errors': {}, 'command': ['synthetic'], 'timeout_seconds': 1}
        for role, name, limit in [('stdout', 'out', 4194304), ('stderr', 'err', 1048576)]:
            value = R.pin(streams / name)
            receipt.update({role + '_file': name, role + '_bytes': value['bytes'], role + '_sha256': value['sha256'], role + '_limit': limit})
        positive('complete synthetic bounded transport', lambda: pub.transport(receipt, streams))
        for name, change in [
            ('nonzero exit', lambda v: v.update(exit=1)),
            ('boolean exit', lambda v: v.update(exit=False)),
            ('timeout', lambda v: v.update(timed_out=True)),
            ('overflow', lambda v: v.update(output_overflow=True)),
            ('partial pipes', lambda v: v.update(pipe_capture_complete=False)),
            ('reader failure', lambda v: v.update(reader_errors={'stdout': ['synthetic']})),
            ('stream traversal', lambda v: v.update(stdout_file='../out')),
            ('changed raw hash', lambda v: v.update(stdout_sha256='0' * 64)),
            ('unbounded deadline', lambda v: v.update(timeout_seconds=61)),
        ]:
            def altered_transport(c=change):
                value = copy.deepcopy(receipt)
                c(value)
                pub.transport(value, streams)
            negative('transport ' + name, altered_transport)
        archive = root / 'archive.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('source/one', b'synthetic archive input')
        digest = hashlib.sha256(b'synthetic archive input').hexdigest()
        manifest = {'sha256': R.pin(archive)['sha256'], 'files': {'source/one': {
            'original_path': 'Z:/synthetic/input', 'bytes': len(b'synthetic archive input'), 'sha256': digest}}}
        meta = root / 'archive.json'
        A.write(meta, manifest)
        expected = {'Z:/synthetic/input': {'bytes': len(b'synthetic archive input'), 'sha256': digest}}
        positive('actual full synthetic archived input join', lambda: pub.archive_readback(meta, archive, expected))
        wrong = copy.deepcopy(expected)
        wrong['Z:/synthetic/input']['sha256'] = '0' * 64
        negative('wrong compiler/archive input join', lambda: pub.archive_readback(meta, archive, wrong))
        bad_meta = copy.deepcopy(manifest)
        bad_meta['files']['source/one']['sha256'] = '0' * 64
        A.write(meta, bad_meta)
        negative('archive summary repair cannot change member', lambda: pub.archive_readback(meta, archive))
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('../escape', b'x')
        A.write(meta, {'sha256': R.pin(archive)['sha256'], 'files': {'../escape': {
            'original_path': 'Z:/synthetic/input', 'bytes': 1, 'sha256': hashlib.sha256(b'x').hexdigest()}}})
        negative('unsafe archive member refused without extraction', lambda: pub.archive_readback(meta, archive))
    print(json.dumps({'status': 'PASS_PORTABLE_SYNTHETIC_OFFLINE_CONTROLS', 'controls': len(results),
        'positive': sum(v['kind'] == 'positive' for v in results), 'negative': sum(v['kind'] == 'negative' for v in results),
        'results': results, 'actual_process_launches': 0, 'JVM_or_native_runs': 0,
        'fake_Popen_calls': len(calls), 'synthetic_data_is_execution_evidence': False}, indent=2))


if __name__ == '__main__':
    main()
