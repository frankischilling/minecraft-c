# Local client statistics files

The `mc_client_stats_writer` owns the real client/user `xi` maps and `cj`
synchronizer. Its directory is the game directory's `stats` child. A World save
directory or new World's null Info.Player is not a replacement for this owner.
The writer retains its exact Registry graph and actual Session identity.
Counted Session holds keep its immutable Strings/class owners live through
joined retirement. Session class live counts count objects, not holds.

Allocate then construct using the actual Session and game-directory path.
Construction publishes total/unsent maps, dirty=false, performs mkdir/root
migration, builds six distinct File objects, applies an existing unsent file,
and starts the actual read thread before publishing the synchronizer field.
Failed prefixes remain inspectable and retireable. Directory enumeration
completes before provider-order relocation begins. Rename/delete booleans and
mkdir failure are discarded where the source discards them.

Presence checks include zero/negative counts and the null map key. Increment
adds unsent first, total second, then stores sticky dirty=true. Addition wraps
as Java int. Service is the actual xi.d operation reached at Minecraft.k PC18:
test the async save gate first, then decrement positive counters and apply/clear
other/main pending maps only after successful application. Volatile condition,
argument reload and field-clear accesses preserve separately retained native
map identities. No server response or successful upload acknowledgment is
invented. Saves never clear dirty or unsent.

Read and save operations create real C17 threads. Native joins do not service
maps, save, change delay or clear busy. A failed launch preserves the earlier
delay=100/busy=true stores. Worker finally paths clear busy even on native
failure; worker errors and caught source exceptions remain distinct. Sync save
waits at most 29 actual 100 ms sleeps, then proceeds even if another worker is
still busy. This is not a join. Native retirement joins every handle before
freeing files, maps, retained Session and Registry graph. A failed join keeps
the owner live. OS sleep signals are retried rather than invented as Java
Thread.interrupt; arbitrary Java thread interruption is outside this seam.

The reader prefers dat/old/tmp, but normal constructor/read-worker outer checks
require dat to exist. It concatenates BufferedReader lines and catches source
I/O/parser failures at their actual boundaries. Syntax errors can return an
empty map and therefore mark dirty when merged. Checksum mismatch returns a
null map and does not mark dirty. Writing opens/truncates tmp before formatting,
uses the real PrintWriter mode without checkError, closes in finally, then
rotates old/dat/tmp. Swallowed print trouble is inspected passively.

JSON uses source CRLFs and raw unescaped user text. Persistence uses current
retained username and literal `local`, never the Session token. MD5 hashes only
the Java UTF-16 length bytes of the selected default-charset encoding and
prints unpadded lowercase hex. The parser preserves duplicate/latest values,
input checksum accumulation, first-field JDK17 list-bin order, signed integer
errors, unknown-stat skipping and checksum diagnostics. Workers read only
immutable statistic class/id/GUID fields, avoiding the independently installed
achievement-description callback cell.

The normal bounded defaults are 64 MiB tracked live storage, 16384 map entries,
4 Mi UTF-16 units, 262144 JSON nodes and 128 nested levels. The normal catalog
roundtrip measures 486 entries and 13470 JSON units. Limits are native resource
policy, not emulated JVM OOM timing. UTF-8/default and the existing supported
reader/writer charset profiles are reused. Filename lowercasing currently
implements an explicit English ASCII username profile; non-ASCII Locale
mapping stops at CJ's first reached toLowerCase at PC51. NUL/path-separator/colon
components and JVM identity-dependent Argo tree bins remain explicit unsupported
profiles. There is no filtering, guessed Unicode case mapping or fake empty map.

Registry initialization/retirement remains exclusive. If parsing first
initializes a cold jl on the read worker, keep that Registry quiescent until
join. The normal READY catalog permits immutable ID/GUID reads while main
startup installs the separate description callback. Public writer mutations
are exclusive; passive state inspection uses the native mutex. Diagnostic
transport may inspect but cannot mutate/reenter/retire the same writer.

The [normal startup owner](normal-startup.md) now adopts the real writer and
callback before invoking the logo and input setup. It does not join or service
the read worker between constructor return and callback installation. Startup
retirement joins before releasing dependent storage. The complete Minecraft
tick, notice GUI, full Player award path and separate V==6000 legacy HTTPS
session check remain unfinished.

The POSIX path adapter treats only slash as a separator. A valid parent ending
in a literal backslash receives an inserted slash before stats, as Java File
does. Windows continues to accept both slash and backslash as separators.
Original and native constructor/save observations exercise this valid POSIX
path without changing the filename profile. Test capture rejects every recorded
pipe/artifact error and requires explicit empty reader-errors; read1 detects
overflow while a child remains asleep. Source-only controls run normally and
with Python optimization enabled.

## Typed source saves

The source File path now uses the canonical typed FileWriter/PrintWriter owner.
It opens and truncates the actual temporary File before serialization, prints
without checkError, closes at the normal/finally source sites, then performs
exists/delete/exists/rename/rename in the original order. Ignored false File
results remain source booleans. Save attempts retain their failed prefixes and
native cleanup remains separate from source close.

The source-file test verifies exact delivered text over two rotations, prior
data moving into the old File, and absence of the temporary File after success.
It also verifies serializer-exception closure without rotation and failed-open
retention. Normal startup's menu caller now reaches this synchronous save.
Throwable printing on the caught save-error path remains an explicit dependency.

File.delete uses one nonrecursive operation. Tests cover missing files,
read-only files, nonempty directory refusal, empty directories, invalid names
and bounds. The Windows provider follows the selected
[JDK 17 implementation](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/java.base/windows/native/libjava/WinNTFileSystem_md.c#L615-L643).
