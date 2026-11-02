# Create World

Singleplayer's Create button now constructs the source `fj` screen with the
current World-selection screen as its parent. The default loop uses the same
App, Display, SaveFormat, Registry and Font owners as the surrounding menus.
Create enters the normal World loader with the derived folder, raw display
name and selected seed.

The checked Windows SDK menu prefix passes 2,349 assertions, including native
drawing, input, Cancel, reopening and cleanup. Component controls pass 42 RO
text-field checks and 207 FJ checks. The full Windows Release run creates
seed 777, completes all 289 terrain samples and its initial save at
`Minecraft.a:339`, returns three World/HUD frames, saves to the title and
reselects the actual generated World for three more frames. The current run
passes 2,844 native checks in 66.93 seconds with independent disk decoding and
checked cleanup. An earlier run passed 2,945 checks in 54.99 seconds before
the diagnostic-only JU223 correction. [Run summaries](../../tests/reference/client/create_world/native-validation.json)
retain their input scope. These results do not establish an executed-original
GUI comparison, a controlled performance comparison or a complete playable session.

## Screen and action order

Initialization obtains the translation singleton, enables keyboard repeat,
clears the inherited button List, and adds Create and Cancel. Both buttons are
200 units wide. The name field starts at `(width/2-100,60)` and the seed field
at `(width/2-100,116)`; each is 200 by 20. Name starts with the translated
`selectWorld.newWorld` String. FJ stores its focus flag directly and then sets
its maximum typed length to 32. Seed starts empty with the RO default maximum
of zero, which permits typing until a native String limit is reached.

The folder helper trims Java UTF16 units at or below U+0020 from the name's
ends, then replaces the 15 characters in the actual `fp.b` array with `_`.
Those characters are slash, LF, CR, Tab, NUL, form feed, backtick, question
mark, asterisk, backslash, `<`, `>`, `|`, double quote and colon. An empty result
becomes `World`. The helper queries the current SaveFormat's metadata lookup
and appends `-` while a WorldInfo exists, including its normal
`level.dat_old` fallback. An empty directory alone is not a metadata collision.
The original text stays in the name field. Create's enabled flag uses its raw
length, so a name containing only spaces can remain enabled while deriving
the folder `World`.

Button actions keep this source order:

| Action | Completed effects and next call |
| --- | --- |
| Disabled button | Returns before any display, guard or seed operation. |
| Cancel, id 1 | Displays the exact retained parent at FJ24. |
| Create, id 0 | Calls display-screen with NULL at42 before checking guard `m` at46. A repeated Create still performs that display call, then returns. |
| First Create | Sets `m=true` at55, constructs a real default Random at62, and consumes `nextLong` at65 before reading the seed field at73. |
| Controller and load | Constructs `os` at127, publishes it in App at130, then calls `Minecraft.a(folder,rawName,seed)` at149. The final display-screen call157 runs only after the loader returns. |

An empty seed keeps the consumed default value. Successful decimal parsing
replaces it only when the parsed long is nonzero; `0`, `-0` and `+000` keep the
default. The seed text is not trimmed. A NumberFormatException selects the
signed, widened Java String hash over the UTF16 units, including overflow and
surrogate units. Other native parsing or Random failures retain their own
diagnostics. The caller does not repair seed state or replay the constructor.

FJ key input first invokes the focused RO. Character 13 then calls Create
synchronously. After a returning action, the old FJ still updates the enabled
flag and recomputes the folder, even if App's current screen changed. Mouse
presses invoke the inherited DA button body before both RO mouse methods.
They likewise finish on the original FJ receiver. Escape has no close override
in this class. Source `h()` disables keyboard repeat when display-screen closes
FJ; native retirement does not invoke that source method.

## Text fields and drawing

RO stores counted UTF16 text, its exact parent and Font, rectangle, enabled and
focus flags, typed-length maximum and signed cursor counter. Setting text
does not truncate it. Backspace removes one UTF16 unit, so it can split a
surrogate pair. Ordinary typing uses the actual `fp.a` whitelist and accepts a
unit when the current length is below the maximum or the maximum is zero.
A negative maximum blocks that typing branch.

Tab invokes the actual parent's `j()` method. FJ toggles the two focus flags;
the RO invocation then continues its remaining statements. Cursor ticks use
Java signed-int wrapping. The focus method resets the counter only on a
false-to-true transition. Mouse focus uses inclusive left/top and exclusive
right/bottom bounds with wrapping integer addition. RO ignores the mouse
button value, and a disabled field loses focus on a mouse call.

Character 22 calls the real DA clipboard bridge only when the enabled, focused
RO reaches paste. The bridge reads SDL text on the video thread and converts
UTF8 to an owned UTF16 Player String. Missing String data returns NULL; RO
substitutes its empty literal. Paste appends at most `32-currentLength` units,
independently of the field's maximum and whitelist. It preserves pasted
control and surrogate units and can end at the first half of a pair. Clipboard
conversion and allocation failures remain native failures.

The SDL adapter supplies characters 9, 13 and 8 for ordinary or Shift Tab,
Return/keypad Enter and Backspace. Ctrl plus a logical Latin letter supplies
1 through 26, including paste22; physical legacy key numbers remain separate.
Release events have character zero and retain the native repeat flag. Alt,
GUI, AltGr, Level5 and other Ctrl combinations remain outside this finite
adaptation. Printable and committed IME text use the existing UTF8-to-UTF16
transport. Raw Windows/X11 message order, timing and complete keyboard-layout
parity are unqualified.

FJ draws its background, five translated labels, name RO, seed RO and inherited
buttons in order. Labels use captured DA Font `g`; each RO uses its own
captured Font `c`. Both call the source Font shadow method. The existing
OpenGL 3.3 canvas supplies background, rectangle and button geometry. The
no-World renderer calls the source PX overlay setup with fresh scaled
resolution before drawing, so source glyph matrices follow resize and GUI
scale. Dirt background disables source lighting and fog. The admitted gradient
branch preserves its source texture, blend, alpha-test and shading sequence.

RO draws the gray outer and black inner rectangles through the inherited UB
bound-swapping rules. Enabled text uses color `0xe0e0e0` and appends `_` when
focused and `(cursor/6)%2==0`, with Java signed division and remainder.
Disabled text uses `0x707070` and passes its nullable String directly. Text
starts at wrapping `x+4`, `y+(height-8)/2`. Failed drawing preserves completed
source state and typed Font/display errors; cleanup adds no source state
rollback.

## Ownership and native limits

FJ owns a retained occurrence of its exact parent. Each initialization record
keeps its reached Buttons and ROs, while the DA initialization records retain
the captured Fonts and App dependencies. Reinitialization preserves the old
records through checked cleanup. RO borrows those parent and Font identities
within its enclosing screen lifetime; it adds no recursive parent hold.
Initial translated text keeps its Registry String identity. Edited fields own
Player String occurrences, and empty edits use the canonical empty literal.

Screen calls use the shared class monitor and an active-call count. Transitions
release that monitor around synchronous calls into other owners, then reacquire
it before continuing the old event. Earlier screens, display calls,
controllers, load calls and failed prefixes remain in the transition graph.
Cleanup refuses active or externally retained owners and permits a later
retry. A failed monitor reacquisition retains a recovery occurrence. Inspectors
borrow live objects; their views end when the corresponding owner retires.

The default retained screen arena is 1 MiB with no separate allocation-count
ceiling. Canonical String children have their own 1,048,576-unit ceiling;
edited text and drawing concatenations report reached limits explicitly.
Transition records have a 4,096-record bound, and the World-load namespace
retains at most eight epochs. Allocation, busy, native-policy and unsupported
dependency failures are distinct from source null/index errors. Diagnostics
retain class, method, descriptor, bytecode position and child results. Completed
field stores, Random consumption and filesystem effects survive failure.

The Create action uses the [ordinary World-loading owner](world-loading.md).
An empty save goes through the actual SaveHandler and fresh World constructor,
then spawn preparation, Player creation/publication and renderer/effect
attachment. New World flag `s` selects the initial source save with its
LoadingScreen at World-change339, before view publication347 and the final
return. Subsequent save/title and reselection use the same retained epoch
machinery as existing saves. Full acceptance of this continuation is pending.

## Evidence and commands

The [static pin packet](../../tests/reference/client/create_world/README.md)
checks the supplied client archive SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
It records 14 complete class inventories, hierarchy, 80 selected method
identities and 185 selected references. The offline verifier checks Code
hashes, exception tables and inherited declarations against those same
archive bytes. It executes no game classes. Missing input returns exit 77;
changed identities or malformed metadata return exit 1.

The RO and FJ component tests use declared native inputs and independent
expected values. They cover nullable fields, String identity, focus, key
ordering, paste limits, wrapping cursor/geometry, all prohibited filename
characters, metadata collisions and default/parsed/hashed seed selection.
Their counts are 42 and 207 checks on the measured Windows build. Those
authored controls are separate from native SDK execution and original
observations.

The Windows SDK menu-only scenario starts with an empty saves directory and
one declared `viewDistance:3` options line. Its 2,349 checks cover five labels,
two fields, four rectangles, two buttons, unchanged Escape behavior, exact
parent Cancel, repeat disable, a distinct reopened screen, typing, Tab and
folder sanitization. Captures were inspected, cleanup completed and both host
windows were absent afterward. This diagnostic creates no World files.
The full scenario additionally requires genuine Enter-created seed 777 terrain,
initial save, returned World/HUD frames, selected live save values and a
distinct World/Player/controller/handler epoch through ordinary reselection.
Its wrapper validates independently decoded saved metadata and region chunks,
retains logs and captures, and checks source/resource continuity. The passing
current run decodes four region files and 375 Chunks, compares every selected Player
field and WorldInfo value to live pre-leave captures, and rejects three
corrupted metadata copies. Both Frame and Canvas are absent after cleanup.
No metadata, terrain, Player pose, statistics, Timer or Random input is supplied;
only the imported resources and one pre-bootstrap Tiny-distance options line
are declared. Natural constructor entropy and scheduling remain actual inputs.

Earlier runs retain the initial-save dependency, missing Tall Grass geometry,
the minimized [Snow cascade](../world/snow.md) stack failure and the reached
[Falling Sand tick](../entity/falling-tick.md) dependency. The first integration
of that tick also exposed reused World diagnostic storage; the dispatcher now
initializes the newly selected retained child before invocation. The passing
full scenario executes the genuine mixed-Entity path with that correction.

With a verified import root containing `jar/` resources and `sdk/` icons, run
these commands from the repository in PowerShell. Adjust `$assets` to the
local import path. The graphical scenario needs a Windows desktop OpenGL
context and uses an isolated game directory under its artifact directory.

```powershell
$assets = (Resolve-Path .local/imported-icons).Path
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=ON -DMC_BUILD_CLIENT=ON `
  -DMC_ENABLE_CLIENT_DISPLAY_TESTS=ON -DMC_ENABLE_CLIENT_CREATE_WORLD_TESTS=ON `
  "-DMC_APPLICATION_STARTUP_IMPORTED_RESOURCES=$assets"
cmake --build build-windows --target mc_test_text_field mc_test_create_world mc_test_create_world_loop
ctest --test-dir build-windows --output-on-failure -R '^client\.(text_field|create_world)$'
python -B tests/reference/client/create_world/verify_source.py --jar b1.7.3.jar
python -B tests/reference/client/create_world/test_verify.py --jar b1.7.3.jar
python -B tests/tools/client/test_create_world_loop.py `
  --executable build-windows/mc_test_create_world_loop.exe --assets $assets `
  --artifacts build-windows/create-world-menu-captures --menu-only
ctest --test-dir build-windows --output-on-failure -R '^client\.create_world_loop$'
```

`MC_ENABLE_CLIENT_CREATE_WORLD_TESTS` defaults to OFF. The model tests remain
ordinary public CTests when client testing is built. The opt-in full graphical
CTest is Windows-only, runs serially and has a 1,200-second timeout. The native
client uses imported assets without a Java runtime. The separate static verifier
reads the locally supplied original archive. Original GUI pixels, clipboard
behavior and the full fresh-session lifecycle have no executed-original
acceptance in this packet.
