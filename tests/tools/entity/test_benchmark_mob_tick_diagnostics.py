"""Pure corruption controls for retained native Mob diagnostic CPU evidence.

Synthetic protocol/link records exercise refusal only; no native process,
compiler, build, resource capture, original runtime or timing is invoked.
"""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('mob_diagnostic_benchmark',
    Path(__file__).with_name('benchmark_mob_tick_diagnostics.py'))
B = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(B)


def payload(variant='both'):
    world, dispatch = B.ROLE_ALGORITHMS[variant]
    result = [dict(event='header', schema=1, variant=variant, clock='qpc', frequency=10000000,
                   ticks_per_interval=16, warmups=2, samples=21, fixture_chunks=25,
                   digest_policy='typed-fnv1a64-le-v1',
                   entropy_exclusions=['living.F', 'living.G', 'living.aj'],
                   world_finalization=world, mob_dispatch=dispatch,
                   sizeof={name: 1024 for name in B.SIZES})]
    for cell, kind, count in B.CELLS:
        for sample in range(23):
            row = dict(event='interval', cell=cell, kind=kind, count=count, sample=sample,
                       warmup=sample < 2, ticks=16, returns=16, start=len(result) * 100,
                       end=len(result) * 100 + 10, status='OK', no_hurt=True,
                       hurt_reached=0, hurt_returned=0, hurt_true=0,
                       dispatch_calls=count * 16,
                       workspace_calls=count * 16 if variant in ('workspace', 'both') else 0,
                       actor_ticks_before=[0] * count, actor_ticks_after=[16] * count,
                       checks=len(result), constructor_unchanged=True,
                       **{name: True for name in B.INVARIANTS},
                       **{name: '0123456789abcdef' for name in B.DIGESTS})
            result.append(row)
    result.append(dict(event='footer', complete=True, rows=161, cleanup=True, checks=200))
    return result


def metadata(directory):
    executables = {name: directory / ('mc_benchmark_mob_tick_' + name) for name in B.VARIANTS}
    workload = directory / 'workload/mob_tick_diagnostics.c.o'
    worlds = {name: directory / ('world/' + name + '.o') for name in ('legacy', 'candidate')}
    dispatches = {name: directory / ('dispatch/' + name + '.o') for name in ('legacy', 'candidate')}
    sources = [(workload, B.ROOT / 'tests/benchmarks/mob_tick_diagnostics.c')]
    for name in ('legacy', 'candidate'):
        sources.extend(((worlds[name], B.ROOT / ('tests/benchmarks/mob_tick/world_' + name + '.c')),
                        (dispatches[name], B.ROOT / ('tests/benchmarks/mob_tick/dispatch_' + name + '.c'))))
    compilation = [dict(directory=str(directory), output=str(obj), file=str(source),
                        arguments=['cc', '-O2', '-fno-fast-math', '-c', str(source), '-o', str(obj)])
                   for obj, source in sources]
    lines = []
    for variant in B.VARIANTS:
        world, dispatch = B.ROLE_ALGORITHMS[variant]
        lines.extend(('build ' + executables[variant].name + ': C_EXECUTABLE_LINKER ' +
                      ' '.join(str(value) for value in (workload, worlds[world], dispatches[dispatch])) +
                      ' | common-a.a common-b.a || order-only-target',
                      '  FLAGS = -O2', '  LINK_FLAGS = -Wl,--gc-sections -Wl,-Map,' +
                      str(executables[variant].with_suffix('.map')),
                      '  LINK_LIBRARIES = common-a.a common-b.a', ''))
    return executables, lines, compilation


class NativeProtocol(unittest.TestCase):
    def parse(self, value, variant='both', suffix='\n'):
        with tempfile.TemporaryDirectory(prefix='mob-diagnostic-protocol-') as directory:
            path = Path(directory) / 'intervals.jsonl'
            path.write_text('\n'.join(json.dumps(row) for row in value) + suffix, encoding='ascii')
            return B.read_rows(path, variant)

    def test_all_four_complete_native_roles(self):
        for variant in B.VARIANTS:
            with self.subTest(variant=variant):
                value = self.parse(payload(variant), variant)
                self.assertEqual(len(value['rows']), 161)
                self.assertEqual(sum(not row['warmup'] for row in value['rows']), 147)
                self.assertEqual(sum(row['returns'] for row in value['rows']), 2576)

    def test_drop_duplicate_reorder_and_incomplete_final_line(self):
        good = payload()
        for value in (good[:-1], good + [good[-1]], good[:5] + good[6:10] + [good[5]] + good[10:]):
            with self.subTest(length=len(value)), self.assertRaises(ValueError):
                self.parse(value)
        with self.assertRaises(ValueError):
            self.parse(good, suffix='')

    def test_header_policy_clock_layout_and_role_corruption(self):
        for key, value in (('schema', True), ('variant', 'legacy'), ('clock', 'process-time'),
                           ('frequency', 0), ('frequency', 1.5), ('warmups', 0), ('samples', 23),
                           ('fixture_chunks', 1), ('entropy_exclusions', ['living.F', 'living.G', 'living.aj', 'health']),
                           ('world_finalization', 'legacy'), ('sizeof', {}), ('extra', 1)):
            bad = payload(); bad[0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.parse(bad)

    def test_duplicate_json_keys_and_nonfinite_constants_refuse(self):
        with self.assertRaises(ValueError):
            B.unique_object([('event', 'header'), ('event', 'footer')])
        for value in (float('nan'), float('inf'), -float('inf')):
            bad = payload(); bad[1]['end'] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.parse(bad)

    def test_every_real_tick_hurt_and_invariant_is_required(self):
        for key, value in (('ticks', 15), ('returns', 15), ('returns', True), ('warmup', False),
                           ('hurt_reached', 1), ('hurt_returned', 1), ('hurt_true', 1),
                           ('no_hurt', False), ('status', 'FAILED'), ('checks', 0),
                           ('world_rng_unchanged', False), ('player_unchanged', False),
                           ('uniquifier_unchanged', False), ('semantic_after', 'bad'),
                           ('chunks_after', 'fedcba9876543210'), ('extra', 1)):
            bad = payload(); bad[1][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.parse(bad)
        for after in ([15], [True], [], [(1 << 31)]):
            bad = payload(); bad[24]['actor_ticks_after'] = after
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.parse(bad)

    def test_positive_nonoverlapping_integer_clocks(self):
        for index, key, value in ((1, 'start', -1), (1, 'end', 100), (1, 'end', 0.5),
                                  (1, 'end', 1 << 63), (2, 'start', 109)):
            bad = payload(); bad[index][key] = value
            with self.subTest(index=index, key=key), self.assertRaises(ValueError):
                self.parse(bad)

    def test_actual_workspace_dispatch_cannot_silently_fall_back(self):
        for variant, key, value in (('both', 'workspace_calls', 0), ('workspace', 'workspace_calls', 15),
                                    ('legacy', 'workspace_calls', 16), ('tail', 'workspace_calls', 16),
                                    ('both', 'dispatch_calls', 15), ('legacy', 'dispatch_calls', True)):
            bad = payload(variant); bad[24][key] = value
            with self.subTest(variant=variant, key=key), self.assertRaises(ValueError):
                self.parse(bad, variant)

    def test_java_tick_wrap_is_accepted_without_work_truncation(self):
        good = payload(); good[24]['actor_ticks_before'] = [(1 << 31) - 2]
        good[24]['actor_ticks_after'] = [-(1 << 31) + 14]
        self.assertEqual(self.parse(good)['rows'][23]['actor_ticks_after'], [-(1 << 31) + 14])

    def test_constructor_source_values_may_evolve_but_are_not_hidden(self):
        good = payload(); good[24]['constructor_after'] = 'fedcba9876543210'
        good[24]['constructor_unchanged'] = False
        self.assertFalse(self.parse(good)['rows'][23]['constructor_unchanged'])
        good[24]['constructor_unchanged'] = True
        with self.assertRaises(ValueError):
            self.parse(good)

    def test_failed_or_unknown_footer_never_qualifies(self):
        for key, value in (('complete', False), ('cleanup', False), ('rows', 160), ('checks', 160), ('extra', True)):
            bad = payload(); bad[-1][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.parse(bad)

    def test_cross_variant_entropy_is_raw_and_only_declared_fields_unpaired(self):
        before, after = self.parse(payload('legacy'), 'legacy'), self.parse(payload())
        after['rows'][23]['raw_before'] = 'fedcba9876543210'
        after['rows'][23]['constructor_before'] = after['rows'][23]['constructor_after'] = '1111111111111111'
        B.compare_semantics(before, after)
        for key in B.PAIRED:
            bad = copy.deepcopy(after); bad['rows'][23][key] = '2222222222222222'
            with self.subTest(key=key), self.assertRaises(ValueError):
                B.compare_semantics(before, bad)

    def test_stdout_requires_actual_empty_stderr_and_complete_fixture_footer(self):
        footer = dict(checks=100)
        good = b'16 achievements\n151 recipes\n' * 161 + b'Mob tick diagnostics: 100 checks; 161 intervals; 2576 FD.g returns\n'
        B.verify_stdout(good, b'', 0, footer)
        for stdout, stderr, code in ((good, b'source failure', 0), (good, b'', 1),
                                     (good.replace(b'2576', b'2352'), b'', 0),
                                     (good + b'ignored extra row\n', b'', 0),
                                     (good.replace(b'16 achievements\n', b'', 1), b'', 0)):
            with self.subTest(code=code, stderr=stderr), self.assertRaises(ValueError):
                B.verify_stdout(stdout, stderr, code, footer)

    def test_complete_factorial_schedule_and_no_success_subset(self):
        schedule = B.schedule()
        self.assertEqual(len(schedule), 16)
        for position in range(4):
            self.assertEqual({schedule[index * 4 + position][1] for index in range(4)}, set(B.VARIANTS))
        processes = [dict(round=index, variant=variant, parsed=self.parse(payload(variant), variant))
                     for index, variant in schedule]
        summary = B.summarize(processes)
        self.assertEqual(len(summary), 7)
        self.assertEqual(summary[0]['variants']['both']['samples'], 84)
        for bad in (processes[:-1], processes + [processes[-1]], list(reversed(processes))):
            with self.assertRaises(ValueError):
                B.summarize(bad)

    def test_semantic_mismatch_refuses_immediately_with_incomplete_cohort_retained(self):
        processes = [dict(round=0, variant='legacy', parsed=self.parse(payload('legacy'), 'legacy'))]
        B.compare_completed_process(processes)
        self.assertEqual(processes[0]['semantic_comparison']['status'], 'PASS')
        processes.append(dict(round=0, variant='tail', parsed=self.parse(payload('tail'), 'tail')))
        processes[-1]['parsed']['rows'][23]['semantic_after'] = 'fedcba9876543210'
        with self.assertRaisesRegex(ValueError, 'pig1 sample0'):
            B.compare_completed_process(processes)
        self.assertEqual(len(processes), 2)
        self.assertEqual(processes[-1]['semantic_comparison']['status'], 'FAIL')
        self.assertEqual(len(processes[-1]['parsed']['rows']), 161)
        with self.assertRaises(ValueError):
            B.summarize(processes)


class LinkBinding(unittest.TestCase):
    def test_only_declared_map_output_difference_is_normalized(self):
        executable = Path('/build/mc_benchmark_mob_tick_both.exe')
        expected = executable.with_suffix('.map')
        for value in ('/NOLOGO /MAP:' + str(expected),
                      '/NOLOGO /MAP:"' + str(expected) + '"',
                      '-Wl,--gc-sections -Wl,-Map,' + str(expected)):
            normalized = B.link_identity({'LINK_FLAGS': value}, 'LINK_FLAGS', executable)
            self.assertIn('<target-map>', normalized)
        for value in ('/NOLOGO /MAP:/build/old.map', '/NOLOGO',
                      '/MAP:' + str(expected) + ' /MAP:' + str(expected)):
            with self.subTest(value=value), self.assertRaises(ValueError):
                B.link_identity({'LINK_FLAGS': value}, 'LINK_FLAGS', executable)

    def test_exact_compiled_factorial_and_escaped_paths(self):
        with tempfile.TemporaryDirectory(prefix='mob-binding-') as name:
            executables, lines, commands = metadata(Path(name))
            binding, products = B.binding_from_metadata(executables, lines, commands)
            self.assertEqual(len(binding['compilation']), 5)
            self.assertEqual(len(products), 7)
            self.assertEqual(B.ninja_words('one$ space path$:name $$literal'),
                             ['one space', 'path:name', '$literal'])

    def test_link_map_rejects_archive_fallback_and_missing_role(self):
        roles = dict(workload='/work/mob_tick_diagnostics.c.obj',
                     world_finalization='/world/world_legacy.c.obj',
                     mob_dispatch='/dispatch/dispatch_candidate.c.obj')
        good = '\n'.join(('mc_world_runtime_tick_entities', 'wt_tick',
                          'mc_benchmark_mob_tick_legacy_tail', 'mc_benchmark_mob_tick_legacy_dispatch',
                          *(Path(value).name for value in roles.values())))
        B.validate_link_map(good, roles)
        for value in (good.replace('dispatch_candidate.c.obj', ''),
                      good + '\nmc_simulation:entity_tick.c.obj',
                      good + '\nmc_simulation.lib:entity_tick_dispatch.c.obj',
                      good + '\nlibmc_simulation.a(entity_tick.c.o)'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                B.validate_link_map(value, roles)

    def test_changed_link_graph_compiler_and_role_object_refuse(self):
        with tempfile.TemporaryDirectory(prefix='mob-binding-') as name:
            executables, lines, commands = metadata(Path(name))
            altered = []
            values = lines.copy(); values[0] = values[0].replace('common-a.a common-b.a', 'common-b.a common-a.a')
            altered.append((values, commands))
            values = lines.copy(); values[5] = values[5].replace('world/candidate.o', 'world/legacy.o').replace('world\\candidate.o', 'world\\legacy.o')
            altered.append((values, commands))
            values = lines.copy(); values[7] = '  LINK_FLAGS = -different'
            altered.append((values, commands))
            changed = copy.deepcopy(commands); changed[3]['arguments'][1] = '-O0'
            altered.append((lines, changed))
            changed = copy.deepcopy(commands); changed[3]['file'] = str(B.ROOT / 'src/world/runtime/entity_tick.c')
            altered.append((lines, changed))
            for values, compilation in altered:
                with self.subTest(values=values[0]), self.assertRaises(ValueError):
                    B.binding_from_metadata(executables, values, compilation)


class AlgorithmManifest(unittest.TestCase):
    def fixture(self, root):
        value = dict(schema=B.ALGORITHM_SCHEMA, legacy_algorithm_source_commit=B.LEGACY_COMMIT,
                     scope=B.ALGORITHM_SCOPE, legacy_algorithms=dict(B.LEGACY_FRAGMENTS),
                     common_observer=B.OBSERVER_SCOPE, production_inputs_at_generation={}, adapters={})
        for name in (*B.PRODUCTION_INPUTS, *B.ADAPTER_INPUTS):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(('/* manifest control source: ' + name + ' */\n').encode('ascii'))
            if name in B.PRODUCTION_INPUTS:
                value['production_inputs_at_generation'][name] = B.digest(path)
            else:
                value['adapters'][name] = dict(bytes=path.stat().st_size, sha256=B.digest(path))
        manifest = root / 'tests/benchmarks/mob_tick/algorithms.json'
        manifest.write_text(json.dumps(value) + '\n', encoding='ascii')
        return manifest, value

    def test_complete_exact_manifest_binds_all_actual_files(self):
        with tempfile.TemporaryDirectory(prefix='mob-algorithm-manifest-') as name:
            root = Path(name); manifest, value = self.fixture(root)
            result, paths = B.validate_algorithms(manifest, root)
            self.assertEqual(result['manifest'], value)
            self.assertEqual(result['sha256'], B.digest(manifest))
            self.assertEqual(len(paths), 7)

    def test_stale_production_and_adapter_content_is_refused(self):
        for path in (*B.PRODUCTION_INPUTS, *B.ADAPTER_INPUTS):
            with self.subTest(path=path), tempfile.TemporaryDirectory(prefix='mob-manifest-stale-') as name:
                root = Path(name); manifest, _ = self.fixture(root)
                with (root / path).open('ab') as target:
                    target.write(b'/* changed after reviewed generation */\n')
                with self.assertRaisesRegex(ValueError, 'Stale .* pin'):
                    B.validate_algorithms(manifest, root)

    def test_stale_pin_or_wrong_role_fragment_schema_is_refused(self):
        edits = (
            lambda value: value['production_inputs_at_generation'].__setitem__(B.PRODUCTION_INPUTS[0], '0' * 64),
            lambda value: value['adapters'][B.ADAPTER_INPUTS[0]].__setitem__('sha256', '0' * 64),
            lambda value: value['adapters'][B.ADAPTER_INPUTS[0]].__setitem__('bytes', True),
            lambda value: value['adapters'].__setitem__('tests/benchmarks/mob_tick/unknown.c', {}),
            lambda value: value['production_inputs_at_generation'].__setitem__('src/unreviewed.c', '0' * 64),
            lambda value: value['legacy_algorithms'].__setitem__('world_tail_sha256', '0' * 64),
            lambda value: value['legacy_algorithms'].__setitem__('ignored_fragment', '0' * 64),
            lambda value: value.__setitem__('legacy_algorithm_source_commit', '0' * 40),
            lambda value: value.__setitem__('scope', 'Historical whole-product speedup'),
            lambda value: value.__setitem__('common_observer', 'Unreviewed observer'),
            lambda value: value.__setitem__('extra', True))
        for index, edit in enumerate(edits):
            with self.subTest(index=index), tempfile.TemporaryDirectory(prefix='mob-manifest-pin-') as name:
                root = Path(name); manifest, value = self.fixture(root)
                edit(value)
                manifest.write_text(json.dumps(value) + '\n', encoding='ascii')
                with self.assertRaises(ValueError):
                    B.validate_algorithms(manifest, root)


class BuildReadiness(unittest.TestCase):
    def fixture(self, root):
        build, artifacts = root / 'build', root / 'artifacts'
        build.mkdir(); artifacts.mkdir()
        (build / 'CMakeFiles').mkdir()
        cmake, ninja = root / 'cmake-tool', root / 'ninja-tool'
        cmake.write_bytes(b'pure fixture tool identity\n'); ninja.write_bytes(b'pure fixture tool identity\n')
        script = build / 'CMakeFiles/VerifyGlobs.cmake'
        script.write_text('# CMAKE generated file: DO NOT EDIT!\n', encoding='ascii')
        stamp = build / 'CMakeFiles/cmake.verify_globs'; stamp.write_bytes(b'glob stamp\n')
        source = root / 'CMakeLists.txt'; source.write_text('# native readiness fixture\n', encoding='ascii')
        cache = build / 'CMakeCache.txt'
        values = dict(CMAKE_BUILD_TYPE='Release', CMAKE_GENERATOR='Ninja', MC_BUILD_BENCHMARKS='ON',
                      BUILD_TESTING='ON', CMAKE_HOME_DIRECTORY=str(B.ROOT), CMAKE_CACHEFILE_DIR=str(build),
                      CMAKE_COMMAND=str(cmake), CMAKE_MAKE_PROGRAM=str(ninja))
        cache.write_text(''.join(key + ':STRING=' + value + '\n' for key, value in values.items()), encoding='utf-8')
        rules = build / 'CMakeFiles/rules.ninja'
        rules.write_text('rule RERUN_CMAKE\n  command = "' + str(cmake) + '" --regenerate-during-build -S' +
                         str(B.ROOT) + ' -B' + str(build) + '\n  generator = 1\n\nrule VERIFY_GLOBS\n' +
                         '  command = "' + str(cmake) + '" -P ' + str(script) + '\n  generator = 1\n', encoding='utf-8')
        escape = lambda value: str(value).replace('$', '$$').replace(' ', '$ ').replace(':', '$:')
        graph = build / 'build.ninja'
        graph.write_text('include CMakeFiles/rules.ninja\n' +
                         'build build.ninja: RERUN_CMAKE CMakeFiles/cmake.verify_globs | CMakeCache.txt ' +
                         escape(source) + ' CMakeFiles/VerifyGlobs.cmake\n' +
                         'build CMakeFiles/cmake.verify_globs: VERIFY_GLOBS | CMakeFiles/VerifyGlobs.cmake_force\n' +
                         'build actor.o: C_COMPILER actor.c\n', encoding='utf-8')
        for path in (cache, source, script, stamp, rules):
            os.utime(path, ns=(1000000000, 1000000000))
        os.utime(graph, ns=(2000000000, 2000000000))
        executables = {variant: build / ('mc_benchmark_mob_tick_' + variant) for variant in B.VARIANTS}
        return build, artifacts, executables, source

    def query(self, command, **unused):
        return subprocess.CompletedProcess(command, 0, b'' if '-P' in command else b'ninja: no work to do.\n', b'')

    def test_exact_graph_and_verified_configuration_admit_without_build(self):
        with tempfile.TemporaryDirectory(prefix='mob-readiness-') as name:
            build, artifacts, executables, _ = self.fixture(Path(name))
            with mock.patch.object(B.subprocess, 'run', side_effect=self.query) as query:
                result, paths = B.verify_readiness(build, executables, artifacts)
            self.assertEqual(result['status'], 'PASS')
            self.assertEqual(query.call_count, 2)
            copied = artifacts / 'readiness/actual-build.ninja'
            self.assertEqual(copied.read_bytes(), (build / 'build.ninja').read_bytes())
            self.assertIn(b'build actor.o: C_COMPILER actor.c', copied.read_bytes())
            self.assertEqual(result['graph_before']['sha256'], result['copied_graph']['sha256'])
            self.assertIn(artifacts / 'readiness/readiness.json', paths)
            self.assertIn('-f', result['commands'][-1]['command'])

    def test_stale_configuration_refuses_before_any_query(self):
        for mode in ('input_mtime', 'cache_mode'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix='mob-stale-config-') as name:
                build, artifacts, executables, source = self.fixture(Path(name))
                if mode == 'input_mtime':
                    os.utime(source, ns=(3000000000, 3000000000))
                else:
                    path = build / 'CMakeCache.txt'
                    path.write_text(path.read_text(encoding='utf-8').replace('=Release', '=Debug'), encoding='utf-8')
                with mock.patch.object(B.subprocess, 'run') as query, self.assertRaises(ValueError):
                    B.verify_readiness(build, executables, artifacts)
                self.assertEqual(query.call_count, 0)
                receipt = json.loads((artifacts / 'readiness/readiness.json').read_text())
                self.assertEqual(receipt['status'], 'FAIL')

    def test_stale_object_dry_run_never_becomes_no_work(self):
        with tempfile.TemporaryDirectory(prefix='mob-stale-object-') as name:
            build, artifacts, executables, _ = self.fixture(Path(name))
            def query(command, **unused):
                stdout = b'' if '-P' in command else b'[1/2] Building C object actor.o\n[2/2] Linking executable\n'
                return subprocess.CompletedProcess(command, 0, stdout, b'')
            with mock.patch.object(B.subprocess, 'run', side_effect=query), self.assertRaisesRegex(ValueError, 'compile/link'):
                B.verify_readiness(build, executables, artifacts)
            self.assertIn(b'Building C object actor.o', (artifacts / 'readiness/ninja-dry-run.stdout').read_bytes())
            self.assertIn(b'build actor.o: C_COMPILER actor.c', (artifacts / 'readiness/actual-build.ninja').read_bytes())

    def test_glob_mismatch_retains_output_and_does_not_query_build(self):
        with tempfile.TemporaryDirectory(prefix='mob-glob-mismatch-') as name:
            build, artifacts, executables, _ = self.fixture(Path(name))
            result = subprocess.CompletedProcess([], 0, b'', b'-- GLOB mismatch!\n')
            with mock.patch.object(B.subprocess, 'run', return_value=result) as query, self.assertRaisesRegex(ValueError, 'glob mismatch'):
                B.verify_readiness(build, executables, artifacts)
            self.assertEqual(query.call_count, 1)
            self.assertEqual((artifacts / 'readiness/verify-globs.stderr').read_bytes(), b'-- GLOB mismatch!\n')

    def test_rewritten_copy_cannot_hide_pending_compile_edges(self):
        with tempfile.TemporaryDirectory(prefix='mob-graph-drift-') as name:
            build, artifacts, executables, _ = self.fixture(Path(name))
            def query(command, **kwargs):
                if '-f' in command:
                    path = Path(command[command.index('-f') + 1])
                    path.write_text(path.read_text().replace('build actor.o: C_COMPILER actor.c\n', ''))
                return self.query(command, **kwargs)
            with mock.patch.object(B.subprocess, 'run', side_effect=query), self.assertRaisesRegex(ValueError, 'graph stayed unchanged'):
                B.verify_readiness(build, executables, artifacts)


class TerrainCopies(unittest.TestCase):
    def fixture(self, root):
        terrain = B.module('mob_canonical_terrain_copy_controls', B.ROOT / 'tests/tools/entity/test_mob_tick.py')
        template, receipt = B.author_terrain_template(terrain, root)
        worlds = [root / 'scene-one/world', root / 'scene-two/world']
        B.copy_terrain_regions(template, worlds, receipt['regions'])
        return terrain, template, worlds, receipt

    def test_real_template_has_independent_byte_identical_copies(self):
        with tempfile.TemporaryDirectory(prefix='mob-terrain-copies-') as name:
            _, template, worlds, receipt = self.fixture(Path(name))
            self.assertEqual(receipt['authoring_calls'], 1)
            self.assertEqual(receipt['producer']['sha256'], B.digest(Path(receipt['producer']['file'])))
            result = B.validate_terrain_copies(template, worlds, receipt['regions'])
            self.assertEqual(result['independent_scenes'], 2)
            self.assertEqual(result['region_files'], 8)
            for relative in B.REGION_NAMES:
                self.assertEqual((template / relative).read_bytes(), (worlds[0] / relative).read_bytes())
                self.assertFalse((template / relative).samefile(worlds[0] / relative))
                self.assertFalse((worlds[0] / relative).samefile(worlds[1] / relative))

    def test_missing_or_changed_template_or_copy_is_refused(self):
        for location in ('template', 'copy'):
            for corruption in ('missing', 'changed'):
                with self.subTest(location=location, corruption=corruption), tempfile.TemporaryDirectory(prefix='mob-terrain-corruption-') as name:
                    _, template, worlds, receipt = self.fixture(Path(name))
                    path = (template if location == 'template' else worlds[1]) / sorted(B.REGION_NAMES)[0]
                    if corruption == 'missing':
                        path.unlink()
                    else:
                        raw = bytearray(path.read_bytes()); raw[-1] ^= 1; path.write_bytes(raw)
                    with self.assertRaises(ValueError):
                        B.validate_terrain_copies(template, worlds, receipt['regions'])

    def test_hardlink_copy_is_refused_even_with_equal_hash(self):
        with tempfile.TemporaryDirectory(prefix='mob-terrain-hardlink-') as name:
            _, template, worlds, receipt = self.fixture(Path(name))
            relative = sorted(B.REGION_NAMES)[0]
            destination = worlds[1] / relative
            destination.unlink()
            try:
                os.link(template / relative, destination)
            except OSError as error:
                self.skipTest('Filesystem cannot author the explicit hardlink corruption: ' + str(error))
            with self.assertRaisesRegex(ValueError, 'hardlink'):
                B.validate_terrain_copies(template, worlds, receipt['regions'])

    def test_full_scene_preparation_calls_actual_producer_once(self):
        with tempfile.TemporaryDirectory(prefix='mob-terrain-preparation-') as name:
            root = Path(name); game = root / 'game'; game.mkdir()
            terrain = B.module('mob_canonical_terrain_prepare_controls', B.ROOT / 'tests/tools/entity/test_mob_tick.py')
            with mock.patch.object(terrain, 'authored_world', wraps=terrain.authored_world) as author:
                template, receipt = B.prepare_terrain(terrain, [game], root)
            self.assertEqual(author.call_count, 1)
            self.assertEqual(receipt['status'], 'PASS')
            self.assertEqual(receipt['copies']['independent_scenes'], 161)
            self.assertEqual(receipt['copies']['region_files'], 644)
            self.assertEqual(B.terrain_region_hashes(template), receipt['regions'])
            self.assertEqual(json.loads((root / 'terrain-preparation.json').read_text())['status'], 'PASS')


if __name__ == '__main__':
    unittest.main()
