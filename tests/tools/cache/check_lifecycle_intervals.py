#!/usr/bin/env python3
"""Check the actual lifecycle interval predicates without original assets or Java.

Compile the validator's own need() calls, with their real helper dependencies,
and verify their enclosing argument and interval bindings. This isolates the
clock predicates; it does not replace full capture or provenance validation.
"""
from __future__ import annotations

import argparse
import ast
import copy
import hashlib
import json
from pathlib import Path
import sys
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[3]
SITES = {
    'trace': ('trace_phase', 'bounded helper epoch interval', 'helper'),
    'states': ('run_states', 'bounded exact integer epoch interval', 'op'),
    'constructor': ('construction_proof', 'ordered bounded constructor epoch', 'row'),
    'preparation': ('preparation', 'preparation sampled epoch interval', 'op'),
    'external_preparation': ('preparation_detach', 'bounded external preparation epoch', 'op'),
    'event': ('trace_phase', 'event inside own helper interval', None),
}
# Exact endpoints from the two retained unload100 operations that exposed the
# old 180-second cap. These are scalar observations, with no original payloads.
RETAINED = (
    ('gcc', 1792347886550, 1792348167828),
    ('clang', 1792347888389, 1792348168466),
)


def require(value, message):
    if not value:
        raise ValueError(message)


def shape(node):
    return ast.dump(node, include_attributes=False)


def parse_expression(text):
    return ast.parse(text, mode='eval').body


def fingerprint(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


class Checks:
    def __init__(self, namespace):
        self.namespace = namespace
        self.positive = 0
        self.negative = 0
        self.failures = []
        self.failure_count = 0

    def failure(self, label, detail):
        self.failure_count += 1
        if len(self.failures) < 64:
            self.failures.append({'case': label, 'detail': detail})

    def expect(self, label, code, environment, accepted):
        try:
            eval(code, self.namespace, environment)
        except self.namespace['ValidationError'] as error:
            actual = False
            detail = str(error)
        except Exception as error:
            self.failure(label, type(error).__name__ + ': ' + str(error))
            return {'accepted': None, 'error': type(error).__name__ + ': ' + str(error)}
        else:
            actual = True
            detail = None
        if actual != accepted:
            self.failure(label, 'expected ' + ('acceptance' if accepted else 'rejection') +
                         ', got ' + ('acceptance' if actual else str(detail)))
        elif actual:
            self.positive += 1
        else:
            self.negative += 1
        return {'accepted': actual, 'error': detail}


def extract(raw, source):
    tree = ast.parse(raw, filename=str(source))
    classes = [n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == '_CaptureValidator']
    require(len(classes) == 1, 'one real capture validator')
    methods = {n.name: n for n in classes[0].body if isinstance(n, ast.FunctionDef)}
    definitions = []
    for name in ('ValidationError', 'need', 'integer'):
        matches = [n for n in tree.body if isinstance(n, (ast.FunctionDef, ast.ClassDef)) and n.name == name]
        require(len(matches) == 1, 'one actual dependency: ' + name)
        definitions.append(copy.deepcopy(matches[0]))
    helpers = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'helper_epoch_limit_ms']
    require(len(helpers) <= 1, 'at most one interval policy')
    definitions.extend(copy.deepcopy(helpers))
    namespace = {'__name__': 'lifecycle_interval_predicates'}
    exec(compile(ast.fix_missing_locations(ast.Module(body=definitions, type_ignores=[])),
                 str(source), 'exec'), namespace)
    sites = {}
    for key, (method, message, binding) in SITES.items():
        require(method in methods, 'real method exists: ' + method)
        fn = methods[method]
        calls = [n for n in ast.walk(fn) if isinstance(n, ast.Call) and isinstance(n.func, ast.Name)
                 and n.func.id == 'need' and len(n.args) == 2 and isinstance(n.args[1], ast.Constant)
                 and n.args[1].value == message]
        require(len(calls) == 1, 'one actual need call: ' + method + '/' + message)
        call = calls[0]
        if binding is not None:
            # The predicate must consume the low/high just unpacked from the
            # actual helper/operation, rather than a separately invented input.
            statements = [(body, index) for parent in ast.walk(fn) for _, body in ast.iter_fields(parent)
                          if isinstance(body, list) for index, statement in enumerate(body)
                          if isinstance(statement, ast.Expr) and statement.value is call]
            require(len(statements) == 1, 'actual expression statement: ' + key)
            body, index = statements[0]
            expected = ast.parse('low,high=' + binding + "['interval_ms']").body[0]
            require(index > 0 and shape(body[index - 1]) == shape(expected), 'actual interval binding: ' + key)
        sites[key] = {'call': call, 'method': fn,
                      'code': compile(ast.Expression(body=copy.deepcopy(call)), str(source), 'eval')}
    return namespace, sites, bool(helpers)


def wiring(checks, sites, has_helper):
    require(has_helper, 'actual scoped helper_epoch_limit_ms exists')
    for key, role in (('trace', "'observed'"), ('states', 'role')):
        fn = sites[key]['method']
        require(ast.unparse(fn.args) == ('self' if key == 'trace' else 'self, role'),
                'actual enclosing method role binding: ' + key)
        expected = parse_expression(
            "need(integer(low,0,1<<63) and integer(high,low,low+helper_epoch_limit_ms(" + role +
            ",self.context,self.scenario))," + repr(SITES[key][1]) + ')')
        require(shape(sites[key]['call']) == shape(expected), 'actual profile/role wiring: ' + key)
    predicates = {
        'constructor': 'integer(low,last,(1<<63)-1) and integer(high,low,low+180000)',
        'preparation': 'integer(low,0,(1<<63)-1) and integer(high,low,low+180000)',
        'external_preparation': 'integer(low,0,(1<<63)-1) and integer(high,low,low+180000)',
        'event': "integer(event['clock_ms'],low,high)",
    }
    for key, predicate in predicates.items():
        expected = parse_expression('need(' + predicate + ',' + repr(SITES[key][1]) + ')')
        require(shape(sites[key]['call']) == shape(expected), 'unchanged own clock predicate: ' + key)


def run(source):
    require(source.is_file() and source.stat().st_size <= 256 * 1024, 'bounded validator source')
    raw = source.read_bytes()
    namespace, sites, has_helper = extract(raw, source)
    checks = Checks(namespace)
    host = SimpleNamespace(context='dedicated', scenario='unload101')
    observed = {'self': host, 'role': 'observed'}
    retained = []
    # Run both real old call sites first. Missing new policy/wiring must never
    # hide the precise old epoch failures in the retained Linux observations.
    for name, low, high in RETAINED:
        for key in ('trace', 'states'):
            result = checks.expect('retained/' + name + '/' + key, sites[key]['code'],
                                   dict(observed, low=low, high=high), True)
            retained.append({'host': name, 'site': key, 'low': low, 'high': high,
                             'duration_ms': high - low, **result})
    try:
        wiring(checks, sites, has_helper)
    except ValueError as error:
        checks.failure('source-wiring', str(error))
        bound_wiring = False
    else:
        bound_wiring = True

    low = RETAINED[0][1]
    for context in ('dedicated', 'client', 'base', None, True):
        for scenario in ('unload101', 'cohort', 'session-world', 'detach-owned', None, True):
            for key in ('trace', 'states'):
                for role in (('observed',) if key == 'trace' else ('observed', 'control', 'preparation', None, True)):
                    limit = 600000 if (role, context, scenario) == ('observed', 'dedicated', 'unload101') else 180000
                    env = {'self': SimpleNamespace(context=context, scenario=scenario), 'role': role, 'low': low}
                    for high in (low, low + 99776, low + limit, low + limit + 1, low - 1,
                                 True, False, float(low), str(low)):
                        checks.expect('scope/' + repr((key, role, context, scenario, high)), sites[key]['code'],
                                      dict(env, high=high), type(high) is int and low <= high <= low + limit)
                    for invalid in (-1, True, False, float(low), str(low)):
                        checks.expect('invalid-low/' + repr((key, role, context, scenario, invalid)),
                                      sites[key]['code'], dict(env, low=invalid, high=low), False)
    for key in ('constructor', 'preparation', 'external_preparation'):
        for high in (low, low + 180000, low + 180001, low + 600000, low - 1, True, float(low)):
            checks.expect(key + '/high/' + repr(high), sites[key]['code'],
                          {'low': low, 'high': high, 'last': low - 1},
                          type(high) is int and low <= high <= low + 180000)
    checks.expect('constructor/prior-order', sites['constructor']['code'],
                  {'low': low, 'high': low, 'last': low + 1}, False)
    for duration in (1, 180000, 600000):
        for clock in (low, low + duration, low - 1, low + duration + 1, True, float(low), str(low)):
            checks.expect('event/' + repr((duration, clock)), sites['event']['code'],
                          {'low': low, 'high': low + duration, 'event': {'clock_ms': clock}},
                          type(clock) is int and low <= clock <= low + duration)
    require(source.read_bytes() == raw, 'source unchanged during predicate checks')
    return {'schema': 1, 'status': 'pass' if checks.failure_count == 0 else 'fail',
            'source': {'path': str(source), **fingerprint(raw)}, 'binding_checks_pass': bound_wiring,
            'retained_operation_results': retained, 'positive': checks.positive, 'negative': checks.negative,
            'failure_count': checks.failure_count, 'failures': checks.failures,
            'actual_callsites': {key: {'method': value['method'].name, 'line': value['call'].lineno,
                                     'expression': ast.unparse(value['call'])} for key, value in sites.items()},
            'scope': 'actual compiled need calls and interval/role/profile bindings; no full capture or provenance replay',
            'no_JVM_C_original_assets_or_child': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'scripts/reference/cache/lifecycle_controls.py')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = run(args.source.resolve())
    except Exception as error:
        result = {'schema': 1, 'status': 'fail', 'error': type(error).__name__ + ': ' + str(error)}
    if args.output:
        with args.output.open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, separators=(',', ':')))
    return 0 if result['status'] == 'pass' else 1


if __name__ == '__main__':
    sys.exit(main())
