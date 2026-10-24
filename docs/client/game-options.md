# GameOption queries and labels

GameOption describes the thirteen options in the original client enum `ht`.
The headless [interface](../../include/mc/client/settings/game_option.h) exposes
their identities, ordinal lookup, translation keys and float/boolean flags.
It also reads values from the real GameSettings owner. Queries do not save or
apply audio, input or rendering changes.

The client-only [label interface](../../src/client/settings/labels.h) composes
option and KeyBinding labels through the shared Assets and Translation owners.
Its native contracts pass on Windows with MSVC. It has no executed-original
label comparison or options-screen integration yet; Linux validation of this
increment remains pending.

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

## Bounds and unfinished work

Assets retains at most 256 cached labels, 65,536 units per key and its existing
8 MiB persistent arena. A composed result permits at most twice that text bound
plus 16 units for separators and numeric text. These are checked native budgets,
not Java allocation-failure timing or String-object identity.

The authored tests cover all descriptors and key slots, label branches,
arithmetic edges, current-array aliases, null/index order, real resource
failures, UTF-16 fallback lifetime and cold singleton/wrapper states.

Native Windows x64 Debug builds with MSVC pass these six CTest cases:

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

These results establish native Windows contracts. They do not establish
original label equality, Linux execution or interactive input. The Translation
constructor's caught-I/O diagnostic, effectful option setters, application
startup/settings use, child menus and full original correspondence remain
unfinished.
