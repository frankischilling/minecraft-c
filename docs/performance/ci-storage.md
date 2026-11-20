# Hosted build performance

## SDK cleanup

The native Linux jobs remove the hosted Android and .NET SDKs while CMake
configures the project. These SDKs are not native build dependencies. Cleanup
is joined before the step returns, and a failed configuration still waits
for it. Configuration and cleanup keep their separate exit-status checks.

The cleanup now deletes each SDK directly. It no longer runs a recursive
`du -sh` immediately before a recursive deletion of the same tree. The
before/after `df` reports still record available workspace storage. The later
build/cache size report remains intact.

The motivating [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/41921975633)
passed all four jobs. Its GCC configure/cleanup step took 324 seconds, while
CMake reported 56.2 seconds configuring and 1.2 seconds generating. After CMake
wrote the build files, the log waited 182.73841 seconds for the Android size
line (`12G`), then 73.951549 seconds until the .NET size line (`5.8G`).

Those intervals contain concurrent cleanup work and do not isolate each
operation's CPU or elapsed cost. They identify a delay in the joined cleanup
after configuration had finished. Removing the duplicate size traversals
does not change the SDK deletion targets, compiler settings, test selection,
sanitizers or runtime. No precise CI speedup or gameplay FPS gain follows
from this log alone; later job durations retain their own runner/cache scope.

## Linking sanitizer executables

Hosted Linux executable links use LLVM lld through the compiler driver. Debug
line tables stay compressed, and the compiler still links the ASan and UBSan
runtimes. Shared library links keep their default linker.

An Ubuntu WSL comparison relinked five storage and World-saving tests from the
same Clang 21.1.8 ASan and UBSan objects. Both linkers compressed debug sections.
Eight alternating pairs ran up to four links at a time on four logical CPUs.
Median batch time fell from 1,297.84 to 427.95 ms with lld 21.1.8. The five
executables grew from 122.85 to 124.84 MB in total. This sample measures local
links for those five targets. Hosted build time also includes compilation,
downloads and cache handling.

## Linking clean Windows Debug executables

Windows CI links Debug executables with `/INCREMENTAL:NO` and keeps `/debug`
for PDBs. The job builds in a fresh directory and restores compiler cache
entries. It has no prior executable or incremental link database to reuse.
Local Debug builds keep the default linker flags.
[MSVC incremental linking](https://learn.microsoft.com/en-us/cpp/build/reference/incremental-link-incrementally?view=msvc-170)

Six alternating Windows MSVC 19.51 pairs linked eight test executables from
the same Debug objects. Each batch wrote fresh outputs and ran up to four
links concurrently. Median batch time fell from 1.486 to 0.627 seconds. Every
pair was faster with `/INCREMENTAL:NO`. The eight builds produced PDBs in
both modes, and the setting eliminated 35.1 MB of incremental link databases
per batch. The sample measures local clean links. Hosted runtime also depends
on runner load, compiler cache restoration and test execution.

## Hex output in storage and population tests

The Chunk storage probe encodes 128-byte blocks before writing to stdout.
The population probe reserves space for each byte array or saved file once,
checks the JSON size limit, and writes lowercase hex directly into its buffer.
Both probes still emit two digits per byte. The named-data and full World save
replays validate the origin snapshot through their comparison functions once
before checking the additional snapshots.

Three alternating Windows MSVC Debug pairs ran the same six Python test
commands with up to four workers. Both sets of probes used the same linker
flags and the same Python checks. All 36 test commands passed.

| Test | Median before | Median after |
| --- | ---: | ---: |
| Chunk storage reference | 28.07 s | 19.19 s |
| Dedicated population replay | 25.68 s | 19.98 s |
| Named-data replay | 23.33 s | 13.59 s |
| Full World save replay | 13.42 s | 9.09 s |
| Save and readback replay | 17.31 s | 9.59 s |
| Population ring replay | 14.57 s | 9.65 s |
| Elapsed batch | 37.94 s | 23.23 s |

Each case ran faster in all three pairs. The figures include Python checks,
native process startup and fixture I/O on this machine. A separate replay
matched the exit status and both output streams for all 275 Chunk probe
requests, including the seven malformed input checks.
