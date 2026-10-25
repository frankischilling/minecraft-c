# GameSettings persistence

The native GameSettings module implements constructor defaults, owned settings
state and `options.txt` load/save. Earlier persistence contracts passed on
MSVC, GCC and Clang. Separate Windows-1252 and explicit Windows UTF-8 comparisons each
match all 11,389 projected persistence fields across 16 original/native cases.
The App now owns and loads these settings at startup before font/menu
consumers; its finite native tests pass on Windows and in the preceding hosted
Linux run. Options menus remain unfinished. The binding-code action's native
contracts pass on all three compilers; no original-action comparison has been
added. The newer float/discrete action contracts pass one Windows MSVC
headless profile: 702 native checks and eight file-check categories. Six paths
stop after assignment at their missing audio/renderer fields. The subsequent
[hosted `2afd39a` checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40154472178)
passes the action contracts on MSVC and sanitized GCC/Clang. Original-action
correspondence and real effects remain open.

The interface is [game_settings.h](../../include/mc/client/settings/game_settings.h).
The [original source contract](../reference/game-settings.md) records the
separate original persistence measurements and unfinished setter effects.
`mc_game_settings` links to `mc_core` and is registered outside
`MC_BUILD_CLIENT`. All three validated build graphs exclude graphics targets,
and both test executables' direct imports exclude graphics libraries.

## State and ownership

The owner exposes all 36 instance roles: 23 scalar/String fields, ten named
KeyBinding references, the current binding array, a borrowed Minecraft receiver
and an owned options File role. The four static label-key tables live in the
separate [GameOption module](game-options.md). The persistence observer does
not emit them.

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

## Application startup

After creating Display, the App obtains a real [GameDirectory](game-directory.md)
and constructs one file-backed settings owner with the App as its borrowed
Minecraft receiver. A caught load failure retains defaults and any earlier
field changes; there is no second-directory fallback or replacement owner.
Native directory/settings failures stop startup.

Layout and resize read that owner's GUI scale. An explicit `--gui-scale`,
including zero, overrides the loaded value after load without saving. Initial
anaglyph reaches the font upload, lazy title texture uploads and FontRenderer
palette. The menu caches this initial flag; no effectful live refresh setter
is supplied. Custom pack selection, audio, source performance pacing, options
screens and physical input remain unfinished. The App performs no automatic
startup or shutdown save.

The native diagnostic adapter emits source messages and counted line text as
UTF-8 with host line separators. It retains NUL, joins surrogate pairs and
substitutes `?` for an unpaired surrogate, without fabricating a Java stack.
The [startup tests](../testing.md#startup-settings-and-game-directory) pass on
Windows MSVC x64 Debug: 57 native directory checks plus six file checks, and
nine real App cases with eight exact decoded-pixel relations. The headless
profile also passes 548 settings checks plus six file checks and 883 option
checks. The later hosted run below passes the directory test on all three
compilers and the startup test on Linux. Earlier isolated persistence
comparisons do not establish directory or original rendered correspondence.

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

## Float and discrete option actions

`mc_game_settings_set_option_float` and `mc_game_settings_set_option` are declared
in [game_option.h](../../include/mc/client/settings/game_option.h). They mutate
the real owner in source order. The float action does not directly save;
every completed discrete path uses the existing admitted save body at PC257,
including a null or float option. Both retain earlier mutations on failure.
The [action contract](game-options.md#float-and-discrete-actions) describes
Java wrapping arithmetic, report preservation and the six missing receiver
fields. `UNIMPLEMENTED_DEPENDENCY` is appended as status 9; prior values and
persistence schemas are unchanged. The new native action target passes
702 checks and eight independent file-check categories on Windows MSVC x64
Debug. The same single headless profile passes the existing 548 settings
checks plus six file checks and 883 GameOption checks, with seven validation
stages and eleven final audits. All 645 C/header files pass formatting. These
results qualify the finite stores, saves, reentry and explicit missing-field
policies; they do not execute real audio or renderer effects. Hosted `2afd39a`
adds GCC/Clang native action coverage within 683 passing cases. Options UI
adoption and original-action comparison remain required; the
[sound resource foundation](sound.md) still needs the real shared SoundManager.

## Binding-code action

`mc_game_settings_set_binding_code` implements the source `kv.a(II)V` action.
It selects the binding from the current array at the supplied signed index,
stores any int32 code, then invokes the real production save body. It does not
select from the named constructor fields or resolve duplicate key codes.
Every reference to the selected binding sees the change. Assigning its existing
code still saves.

The source method has no catch table. A null array fails at `aaload` PC5 before
the index check; a negative or excessive index fails at the same instruction.
A null selected binding fails at `putfield` PC7. Those paths change no binding
and never reach save. A valid store precedes the save call at PC11, followed by
normal return PC14 if save returns. The noarg owner's null File still reaches
the real save and its existing failure handler.

The action returns `MC_GAME_SETTINGS_SOURCE_EXCEPTION` for those uncaught
null/index failures. Its report records the reached PC, action exception,
whether assignment and save were reached, and the actual nested save report.
Save's caught failures still return native `OK`; a native allocation, budget,
diagnostic or cleanup failure propagates separately. Neither case rolls back
the assigned code or earlier file effects. The report does not fabricate a
Java Throwable or turn a normal return into proof of complete I/O.

The action and save share one admitted save body. The owner remains active
through assignment, diagnostics and writer retirement, so nested mutation,
load/save, another action or owner retirement returns `BUSY`. Diagnostics may
read the code already assigned. Native receiver/BUSY admission leaves the
action report unchanged; reached work reports its prefix even on failure.

The existing `mc_game_settings_binding_set` remains a raw name/code field edit
with no save. The new action adds no audio, renderer, key-name or Controls UI
callback. Controls adoption, pending-key selection policy and an original
action comparison remain separate work. Fresh Windows MSVC x64 Debug tests
pass 548 native settings assertions and six independent file checks, including
the real-file, alias, same-code-save, partial-write and reentry cases. The later
hosted checkpoint below also passes the native contracts on Linux GCC/Clang.

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
Its Python driver independently checks six file outcomes. The earlier
persistence-only MSVC, GCC and Clang builds each pass 481 native checks and
those six file checks through
`client.game_settings`. The input-driven probe also passes all 26 transport
controls through `tools.client_game_settings_probe` on each compiler. All six
CTest cases pass without failures or skips. Linux enables ASan, UBSan and leak
checks. That checkpoint passes 635-file clang-format 18 checks. These counts
precede the new binding-action tests.

That tested source snapshot contains 1,199 files, including 1,024 non-Markdown
files. A separate file-only audit rehashes 1,706 consumed artifacts and joins
the actual raw output, JUnit, individual receipts, complete file/directory
trees and all four build products per compiler. Eight Linux products are
retained by a later file-only copy with their original before/after hashes.
That copy executes no tests or product binaries.

The later [hosted run for `ea1aefc`](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841)
passes 224 MSVC, 228 GCC and 228 Clang cases, 680 in total, with zero failures,
skips or missing runs. Formatting covers 644 C/header files. Its audit rejoins
all 1,213 literal source bodies and 1,035 non-Markdown tested inputs. This run
includes native binding-action/query/label contracts, GameDirectory on all
three compilers, and startup graphics tests on Linux. It precedes the new
float/discrete action bodies and supplies no original-label, original-action,
ordinary-default or original-startup comparison.

Windows configure and build use recorded cleanup of the pinned compiler
helper; those two process trees are qualified rather than natural. The test
tree and six other Windows tool stages close naturally. Linux records normal
kernel exits, final direct reaps, empty owned children and closed PIDFDs.
These checks establish the declared native contracts. The separate original
comparisons below qualify only their stated persistence projection.

Build and run the two settings tests without a display, original archives or
Java:

```sh
cmake -S . -B build -DMC_BUILD_CLIENT=OFF -DBUILD_TESTING=ON
cmake --build build --config Debug --target mc_test_game_settings mc_game_settings_probe
ctest --test-dir build -C Debug --output-on-failure -R '^(client[.]game_settings|tools[.]client_game_settings_probe)$'
```

## Finite original comparisons

Two Windows profiles use the same 16 authored cases and MSVC probe: default
windows-1252 and explicit UTF-8, both with CRLF output. Each comparison matches
11,389 fields across 64 paired stages, including 2,304 instance-role pairs and
2,877 logical reference joins. Complete file bytes, float bits, UTF16 units,
binding-array order and repeated aliases are compared. No expected settings
state or saved file enters the native producer.

Fourteen file-only controls on the Windows-1252 baseline reject their intended
changes. Twelve repeat the full comparison on copied records; two exercise
explicit preparation-inventory and source-manifest admission checks. All full
evaluations pass their consumed-file after-checks. The controls start no target
or JVM, and the original evidence remains unchanged.

The separate GCC Linux UTF-8/LF comparison also matches all 11,389 fields in
the 16-case profile. Clang Linux comparison remains pending. The
[reference record](../reference/game-settings.md#finite-native-comparisons)
defines each profile and its exclusions. The four static label arrays,
source Thread/Throwable behavior, late-write and mid-read failures, effectful
setters and the new application connection remain outside these finite results.

The separate [query and label module](game-options.md) uses the real current
binding array and Translation owner. Its native contracts pass on MSVC and
sanitized Linux GCC/Clang in the preceding hosted checkpoint. Original-label
correspondence remains pending.
Direct field assignments remain separate from the new option actions.
SoundManager changes, RenderEngine texture refresh, RenderGlobal rebuild and
physical input mapping remain unfinished; the new actions stop before those
receiver fields can be read. Applying changes through Options menus still
requires the real effects and UI. Initial startup use has the finite Windows
and hosted Linux validation above; broader native/default-path and original
startup correspondence remain unqualified.
Persistence alone does not qualify complete settings or client behavior.
