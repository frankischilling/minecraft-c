# GameOption values, labels and actions

GameOption describes the thirteen options in the original client enum `ht`.
The headless [interface](../../include/mc/client/settings/game_option.h) exposes
their identities, ordinal lookup, translation keys and float/boolean flags.
It also reads values from the real GameSettings owner. Queries do not save or
apply audio, input or rendering changes.

The client-only [label interface](../../src/client/settings/labels.h) composes
option and KeyBinding labels through the shared Assets and Translation owners.
Its native contracts pass on Windows with MSVC and on sanitized Linux GCC and
Clang in the hosted checkpoint below. It has no executed-original label
comparison or options-screen integration yet.

## Values and source failures

Ordinal lookup returns null for an ID outside 0 through 12. It does not clamp or
wrap. The float query reads music, sound or sensitivity and returns positive
zero for other options, including null. The boolean query reads invert mouse,
view bobbing, anaglyph, advanced OpenGL or ambient occlusion. It returns false
for the other options, including GRAPHICS; a null option represents the source
null-ordinal failure. Fancy graphics is read separately when composing its
label.

Render distance, difficulty, GUI scale and performance use their original four
translation-key tables. Labeling reads the stored index directly. A negative
or excessive value fails at the reached array access after translating the
prefix. Loading an out-of-range value remains permitted. Unrelated invalid
settings are not checked in advance.

Music and sound display the translated off label for either signed zero.
Otherwise, labeling multiplies in binary32 by 100, applies Java float-to-int
conversion and appends a decimal integer and percent sign. Sensitivity uses
the translated minimum and maximum at zero and one; other values use a
binary32 multiplication by 200. There is no slider clamp. NaN formats as 0%;
infinities saturate to the signed integer endpoints. The translated prefix is
followed by exactly `: ` and the suffix, without a newline.

## Translation order and text ownership

Action and option labels first admit the actual `nh` singleton. A later null
option, binding array, array index, binding or binding name therefore retains
the initialization already reached. The performance suffix alone reaches the
separate `do` wrapper, after its raw index succeeds. A direct lookup that
previously cached the same key does not skip wrapper initialization.

Assets accepts nullable counted UTF-16 keys. A nonnull empty key differs from a
null key; NUL and lone surrogate units are retained. Properties lookup uses the
real shared Translation. Missing keys get an owned copy of the complete key,
and successful resource values borrow the Registry's retained String. Cache
keys compare by length and all units. The existing ASCII convenience function
uses the direct singleton path, including the title screen's five menu keys.

The label result owns its UTF-16 buffer and survives Settings and Assets
retirement. Free it with `mc_game_settings_label_free`. A successful key-name
hole has `is_null=true`; a successful empty String has `is_null=false` and zero
length. Native argument, allocation and limit errors remain separate from
represented source null/index failures. Failure preserves the caller's result
while retaining reached initialization and resource effects.

## KeyBinding and key names

Both binding queries read the current binding array, rather than the ten named
constructor fields. Replacement arrays, repeated aliases and null entries keep
their ordinary GameSettings meaning. The action label translates the selected
binding's current name. The key label reads its current code without admitting
Translation.

Key names use the retained LWJGL 2.9.0 constant table after normal static
initialization: 256 slots, 131 names and 125 null holes. Valid holes return null;
codes below zero or above 255 represent the source array-index failure. NONE is
at zero and LMETA/RMETA at 219/220. These facts do not implement a keyboard
device, SDL event mapping, platform key discovery, physical input or exceptional
partial Java initialization.

Changing a code is a separate
[GameSettings action](game-settings.md#binding-code-action): it updates the
current array's binding and calls real save. Label queries do not perform that
action, and the raw binding field mutator still does not save. The new action's
native contracts pass separately; these query results do not qualify a
Controls screen.

## Float and discrete actions

The headless interface now declares `mc_game_settings_set_option_float` and
`mc_game_settings_set_option`. Both accept a real descriptor or null. Foreign
nonnull descriptors are rejected by identity before mutation. Native admission,
foreign-descriptor and BUSY failures preserve the caller's action report.
The owner stays active across assignment and any real nested save, so diagnostic
reentry and retirement remain BUSY.

The float action stores music, sound or sensitivity without arithmetic, finite
filters or slider clamps. Null and other options return at PC56 without a
store or direct save. A float's received representation is copied; this does
not prove that an original signaling-NaN argument preserved its requested bits
before reaching the method. Sensitivity completes without an effect dependency.

Discrete comparisons retain their source order. Distance, GUI scale and
difficulty use Java wrapping addition followed by `& 3`; performance wraps
both additions in `(old + step + 3) % 3` and can retain a negative remainder.
Every completed path, including null and float descriptors, invokes the real
save body at PC257 and reaches PC260 on native success. Source-caught save
errors remain in its nested report. A later native failure retains assignment
and file effects rather than rolling them back.

Music/sound now call an attached concrete [SoundManager](sound-manager.md)
after field reads at PCs 16/38. Setup can perform its two nested production
saves while public diagnostic reentry remains BUSY. Unattached music/sound,
advanced OpenGL, graphics and ambient occlusion reach
RenderGlobal at PCs 111/217/250; anaglyph reaches RenderEngine at PC 144. Null Minecraft
returns `SOURCE_EXCEPTION` with `NULL_MINECRAFT`. A nonnull opaque receiver
returns the appended status 9, `UNIMPLEMENTED_DEPENDENCY`, at that same first
unreadable field access. Those unresolved paths perform no pointer cast,
effect callback or later save.
Existing status 0..8 values and persistence schemas remain unchanged.

The report records `source_pc`, `value_assigned`, `sound_manager_reached`, `effect_dependency`,
`uncaught_exception`, `save_reached` and the real nested save report. These PCs
are [static bytecode provenance](../reference/game-settings.md#setters-and-unfinished-effects),
not observed original execution PCs. One Windows MSVC x64 Debug headless
profile now passes 702 native action checks and eight independent file-check
categories, plus 548 persistence checks/six file checks and 883 metadata/query
checks. It completes seven stages and eleven final audits. Test children close
naturally; configure/build retain the explicit pinned compiler-helper allowance.
The tested source has 1,215 public members, 1,037 non-Markdown inputs and
645 C/header files, all formatter-checked before the target run. There is no
new local client-mode or Linux action run. The subsequent
[hosted checkpoint `2afd39a`](https://github.com/frankischilling/minecraft-c/actions/runs/40154472178)
passes these contracts on MSVC and sanitized GCC/Clang within all 683 cases.
Actual audio, texture refresh, RenderGlobal rebuild, Options UI and original
actions remain unqualified. The concrete SoundManager now satisfies the native
audio option-call boundary; backend playback and UI adoption remain unfinished.

## Bounds and unfinished work

Assets retains at most 256 cached labels, 65,536 units per key and its existing
8 MiB persistent arena. A composed result permits at most twice that text bound
plus 16 units for separators and numeric text. These are checked native budgets,
not Java allocation-failure timing or String-object identity.

The authored tests cover all descriptors and key slots, label branches,
arithmetic edges, current-array aliases, null/index order, real resource
failures, UTF-16 fallback lifetime and cold singleton/wrapper states.

The earlier local Windows x64 Debug builds with MSVC pass these six CTest cases:

| Case | Observed checks |
| --- | ---: |
| `client.game_option` | 883 metadata/query checks |
| `client.game_settings_labels` | 1,241 label checks |
| `client.assets` | 463 resource-owner checks |
| `resources.translation` | 4,404 translation checks |
| `client.game_settings` | 481 native and six independent file checks |
| `tools.client_game_settings_probe` | 26 transport controls |

All three headless cases completed successfully, then their validation
controller misread the phrase `0 tests failed` as a failure. That failed report
is retained. A separate file-only audit qualifies the three completed tests,
their raw/JUnit records, real file outcomes and all 26 controls without repeating
execution. The after-test inventory command did not run in that profile. The
corrected client controller completes all seven stages and its three cases.
Both builds retain their qualified compiler-helper cleanup; test processes
close naturally. All 641 C/header files pass clang-format 18.1.3.

The later [hosted run for `ea1aefc`](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841)
passes all 224 MSVC, 228 GCC and 228 Clang cases, including these six native
test families. All 680 cases have complete Start/Passed records without
failures, skips or missing runs. Linux runs with ASan/UBSan; all 644 C/header
files pass formatting. The audit rejoins all 1,213 literal publication bodies
and 1,035 non-Markdown tested inputs. It precedes the new float/discrete actions.
The earlier local controller failure and unrun after-test inventory remain
historical facts; the hosted run is separate.

These results establish native contracts on all three compilers. They do not
establish original label equality or interactive input. The App now loads real
settings before title consumers and applies initial GUI scale and anaglyph;
that [startup connection](startup.md#settings-at-startup) passes nine Windows
App cases and eight exact decoded-pixel relations. The separate headless run
also passes all 883 option queries. The hosted run passes the directory test
on all three compilers and the startup test on Linux. The App still draws
Default resources. The Translation constructor's caught-I/O diagnostic, real
option-effect receivers, pack selection, audio, child menus, physical key
mapping and full original correspondence remain unfinished.
