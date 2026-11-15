# Parallel World query controls

The 56 cases registered by
[world_query_controls.cmake](../../cmake/tests/world_query_controls.cmake)
now run without a shared CTest resource lock. Each invocation already creates
an atomic, unique temporary directory beneath its work directory. The Python
runner writes its owned fixtures, captures and reports there, passes that
absolute root to the native process, and uses it as the process working directory.
The native fixtures construct their JavaFile and World below the supplied root.

Shared source files, bundles and native libraries are read and hashed. The
audit found no shared mutable file, port or process environment in these cases;
all 56 retained paths from the preceding run are distinct. The change removes
only this registration loop's lock. Its labels, 420-second timeout, commands,
assertions and file guards remain unchanged. Other users of the same lock keep
their existing scheduling.

## Observed Windows timing

Both observations use native MSVC Release products and eight CTest jobs.

| Observation | Cases | Wall time | Summed case time |
| --- | ---: | ---: | ---: |
| Locked cases within the preceding broad run | 56 | Interleaved with other lock holders | 462.03 seconds |
| Unlocked focused rerun | 56 | 52.54 seconds | 419.38 seconds |

All 56 unlocked cases pass, with their public source inputs unchanged through
the run. The preceding broad run's final 81 serial starts consumed 262.85
seconds, including 235.37 seconds in this query loop. Those observations explain
why the lock delayed the suite despite its parallel job setting.

The broad and focused runs have different workload contexts. Full-suite and
hosted timings retain their own records in the [test guide](../testing.md#fire-source-bodies).
The cases still hash roughly 298 MB across 6,105 protected source files twice
per invocation; removing the lock preserves those guards. This measurement
covers test scheduling, with gameplay and frame-rate measurements kept separate.

The broader eight-job Windows runs take 688.46 seconds before this change and
364.40 seconds after it. The first run has one obsolete plant-frontier
assertion; the revised fixture and all 1,256 cases pass in the second run.
These runs have different concurrent workloads, so the figures record observed
suite times rather than an isolated estimate of the lock's entire effect.
