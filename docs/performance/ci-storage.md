# Hosted Linux build performance

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
