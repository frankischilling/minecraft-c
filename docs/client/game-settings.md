# GameSettings persistence

The native GameSettings module implements constructor defaults, owned settings
state and `options.txt` load/save. Its native contracts pass on MSVC, GCC and
Clang. Comparison with the retained original observations remains pending.
The client application does not yet use this module for its options menus or
startup settings.

The interface is [game_settings.h](../../include/mc/client/settings/game_settings.h).
The [original source contract](../reference/game-settings.md) records the
separate original persistence measurements and unfinished setter effects.
`mc_game_settings` links to `mc_core` and is registered outside
`MC_BUILD_CLIENT`. All three validated build graphs exclude graphics targets,
and both test executables' direct imports exclude graphics libraries.

## State and ownership

The owner exposes all 36 instance roles: 23 scalar/String fields, ten named
KeyBinding references, the current binding array, a borrowed Minecraft receiver
and an owned options File role. The four static label arrays are outside this
interface. They are not replaced with invented native label state.

Defaults include music/sound volume 1, sensitivity 0.5, distance/GUI scale 0,
performance mode 1, difficulty 2, view bobbing/fancy graphics/ambient occlusion
enabled, texture pack `Default` and an empty last-server String. The other
boolean fields start false, and source G/H start at 1. D/G/H retain their source
letters where their complete gameplay meaning is unresolved. Texture pack is
the selected pack name; performance mode retains its source integer meaning.

Each constructor creates ten distinct KeyBinding owners. Named fields and the
array hold references to those same objects, in different orders:

| References | Order |
| --- | --- |
| Named fields m through v | forward, left, back, right, jump, inventory, drop, chat, fog, sneak |
| Initial array w | forward, left, back, right, jump, sneak, drop, inventory, chat, fog |
| Initial array key codes | 17,30,31,32,57,42,16,18,20,33 |

Changing a binding's code or name is visible through every alias to that
binding. Replacing a named field does not rebuild the current array. The array
can be replaced with references to owned bindings, including duplicates and
null entries; a null array and a nonnull empty array remain distinct. Additional
binding owners can be created within the native budget. Foreign binding
references are rejected before dereference.

`mc_game_settings_read_state` and `mc_key_binding_read` return borrowed views.
Owned nonempty String storage and earlier array views remain live until the
settings owner is retired, including through later loads and assignments.
Default name/pack literals are shared across constructors. Empty String values
use canonical storage; distinct externally created empty Java String identities
are outside this interface. Null Strings remain distinct from empty Strings.
External text is copied as counted UTF16, including NUL and lone surrogates.

All access requires exclusive ownership. A diagnostic callback may inspect the
receiver, but mutation, load/save or retirement during that callback returns
`BUSY`. `mc_game_settings_retire` frees retained memory and clears the caller's
owner slot. It performs no save, encoding or source stream close.

## Construction and loading

`mc_game_settings_create_defaults` establishes defaults without I/O and leaves
the Minecraft receiver and File role null. File-backed construction establishes
the same defaults, stores the borrowed receiver, owns the options path and
loads it. It does not create a directory or options file. Persistence never
dereferences the borrowed Minecraft receiver, so this subset admits null.

Native filenames use UTF8, with strict conversion to Windows UTF16. A null
directory argument selects relative `options.txt`; a nonnull empty parent
follows the source File constructor's platform default parent. Windows `C:`
remains drive-relative. General Java File normalization, SecurityManager and
arbitrary UTF16 filename behavior are outside this native path interface.

Load opens the actual file and uses the public Reader module. Input is
unbuffered at the native file adapter. On Linux, the opened descriptor is
checked with `fstat`; a directory is rejected with `EISDIR` before Reader
allocation or a successful source-open report. Closing that rejected native
descriptor is recorded separately from source close.

Lines end at CR, LF or CRLF, and an unterminated final line is retained. Decoding
uses the selected charset's replacement behavior. There is no BOM removal,
key trimming, case folding, escaping or comment syntax. A colon split keeps
interior empty values and discards trailing empties. Unknown keys are ignored;
duplicates apply in order. For example, `skin::extra` assigns an empty pack,
`skin:` is a bad option, `lastServer:` retains the previous value, and
`lastServer:host:port` stores only `host`.

Boolean fields accept only exact lowercase `true` as true. The three floats
first recognize raw `true`/`false`, then use the public Java float parser.
Integers use the public UTF16 decimal parser, including its selected Unicode
digit profile and signed32 checks. Loaded numbers retain their values outside
UI ranges; NaN, infinities and signed zero are not clamped.

The setting comparisons run before the current binding-array loop. A null array
or entry can therefore fail a line after a scalar or earlier binding has
already changed. Each bad line emits `Skipping bad option: ` with the complete
decoded original line, then load continues. An outer file/null-File failure
emits `Failed to load options` and retains earlier field effects. The explicit
source Reader close is reached at EOF; failing paths do not acquire an invented
source finally block.

## Saving and failures

Save opens the real non-append FileWriter before allocating the line workspace
or formatting a value. The file may already be created or truncated when a
later failure occurs. There is no temporary transaction, backup, parent mkdir,
rename or rollback.

The saved setting order is `music`, `sound`, `invertYMouse`, `mouseSensitivity`,
`viewDistance`, `guiScale`, `bobView`, `anaglyph3d`, `advancedOpengl`, `fpsLimit`,
`difficulty`, `fancyGraphics`, `ao`, `skin`, `lastServer`. Save then visits the
current binding array and writes `key_` plus each nullable name and its code.
The constructor array yields 25 lines, including the final line separator.
Unknown input keys are discarded.

Floats use the public selected-JDK17 legacy formatter, integers use signed
decimal and booleans use lowercase text. Strings are appended without escaping;
a null String prints `null`. Colons, embedded newlines and encoding replacement
can make a later reload differ from the state that was saved.

The [Writer module](../core/java-writer.md) supplies PrintWriter behavior with
autoFlush false. GameSettings never calls `checkError`. Ordinary write/close
IOException behavior can be swallowed into trouble, and an interrupted write
can request reinterruption. `mc_game_settings_report` retains the passive native
Writer state, including its explicit reinterrupt request; this is not a claim
to observe a Java Thread flag. A normal save return does not prove complete I/O.

A null array or binding reaches the outer save failure after earlier println
calls. Already committed bytes remain. Native retirement closes an unclosed
sink without flushing the encoder or encoding its pending text. Reports keep
source-close reachability, additional native close and cleanup errors separate.

Caught source failures return native `OK` with the diagnostic/report. Native
allocation, limit, unsupported-charset, diagnostic-delivery and cleanup failures
propagate separately. No Java OutOfMemoryError timing or Java stack trace is
fabricated. The optional diagnostic adapter receives the source message and
cause category; a null adapter leaves the report available without global I/O.
Its original-line view is borrowed only for the callback.

Profiles support UTF-8, windows-1252, ISO-8859-1 and US-ASCII with LF or CRLF output.
Pass the measured charset/separator explicitly for reference work. `DEFAULT`
uses native discovery at Reader/Writer admission and does not model Java's
globally cached defaultCharset or arbitrary file.encoding changes.

## Bounds and native validation

| Native resource | Limit |
| --- | ---: |
| One String or decoded line | 1,048,576 UTF16 units |
| Native path | 1 MiB |
| Input bytes | 16 MiB, plus one measured overflow byte |
| Retained owner arena | 16 MiB |
| Temporary Reader arena per load | 32 MiB |
| Current binding-array slots | 1,024 |
| Binding owners | 256 |

The save workspace has room for one maximal String plus its bounded prefix and
integer suffix. Arithmetic and aggregate allocations are checked. An aggregate
limit can be reached before a more specific limit. Limits preserve earlier
field/file effects and are native resource admissions, not UI constraints or
simulated Java allocation failures. File I/O is synchronous; validation uses an
outer process owner with finite time and output limits.

The authored `client.game_settings` test covers defaults and binding aliases,
ordered load/save, malformed/Unicode text, retained partial effects, diagnostics
and reentry, real Unicode filenames, directory/missing-parent failures, actual
truncation and an emitted prefix without source finally, and native limits.
Its Python driver independently checks six file outcomes. Each actual MSVC,
GCC and Clang build passes 481 native checks and those six file checks through
`client.game_settings`. The input-driven probe also passes all 26 transport
controls through `tools.client_game_settings_probe` on each compiler. All six
CTest cases pass without failures or skips. Linux enables ASan, UBSan and leak
checks. All 635 C files and headers pass clang-format 18.

The tested source snapshot contains 1,199 files, including 1,024 non-Markdown
files. A separate file-only audit rehashes 1,706 consumed artifacts and joins
the actual raw output, JUnit, individual receipts, complete file/directory
trees and all four build products per compiler. Eight Linux products are
retained by a later file-only copy with their original before/after hashes.
That copy executes no tests or product binaries.

Windows configure and build use recorded cleanup of the pinned compiler
helper; those two process trees are qualified rather than natural. The test
tree and six other Windows tool stages close naturally. Linux records normal
kernel exits, final direct reaps, empty owned children and closed PIDFDs.
These checks establish the declared native contracts, not original persistence
or whole-client equality.

Build and run the two settings tests without a display, original archives or
Java:

```sh
cmake -S . -B build -DMC_BUILD_CLIENT=OFF -DBUILD_TESTING=ON
cmake --build build --config Debug --target mc_test_game_settings mc_game_settings_probe
ctest --test-dir build -C Debug --output-on-failure -R '^(client[.]game_settings|tools[.]client_game_settings_probe)$'
```

Direct field-assignment functions do not implement the effectful GameSettings
option setters. SoundManager changes, RenderEngine texture refresh,
RenderGlobal rebuild, translation/labels and physical key-name lookup remain
unfinished. Applying values to the real client and implementing the options
menus require those bodies and their source order; persistence alone does not
qualify complete settings or client behavior.
