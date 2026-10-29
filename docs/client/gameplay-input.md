# Gameplay input and controller ticks

The default source loop executes `Minecraft.k` through picking, the admitted
chunk-provider branch, the single-player controller tick and healthy Player
screen checks. With no screen, it executes gameplay input through bytecode
1073. The [World segment](renderer-ticking.md) now handles paused ticks and
composes renderer/entity work before unfinished scheduling at1260. Dynamic
terrain drawing and a playable frame remain unfinished.

## Source order

`src/client/runtime/gameplay_input.c` consumes the existing SDK Mouse queue,
then the Keyboard queue. It does not pump SDL or create another input queue.
Each mouse event reads the real wall clock and subtracts App.P with Java long
wrapping. An age greater than200ms discards the entire event, including wheel
and button work. Negative elapsed time is admitted, as in the original.

Wheel events change the actual Player inventory before reading Settings.D.
When D is true, the same Settings.G receives the normalized wheel sign times
0.25f. An unfocused button press invokes the genuine focus body and skips the
click action for that event. Screen events use the same retained transition
namespace as ordinary screen input.

Every keyboard event reaches the actual Player movement input before the
pressed-state test. Releases therefore clear movement keys without triggering
pressed-only actions. Number keys select the real nine-slot hotbar. F1, F3,
F5 and F8 toggle the source Settings fields without saving. The fog binding
invokes the existing render-distance option action and its save behavior.
Named bindings are read from their current owners rather than copied codes.
Key dispatch does not itself update movement axes or move the Player.

Fullscreen, pause, inventory/chat screens, sound reload, item dropping,
click/use, pick-block and progressive mining retain their reached dependencies.
Held-button checks retain the original short-circuit order and Timer rate.
The non-mining path executes the real controller reset; it does not replace
unfinished mining with a successful no-op.

## Controller and lifetime

The actual single-player `os.c()V` copies f to g without float arithmetic,
then reads its inherited App and actual App.B before calling SoundManager's
music tick. `os.a()V` stores positive zero in f and zero in i. Other controller
fields remain unchanged. These bodies use the existing controller owner and
SoundManager, including the shared Random and backend state.

Each controller tick retains its own diagnostic call and native controller
occurrence. A nonempty output slot is rejected before allocation or source
effects. Failures preserve completed stores and child diagnostics. Native
retirement does not replay source operations or restore fields.

Gameplay failures leave later SDK events unread. Focus and screen-monitor
prefixes have checked cleanup. Value diagnostics survive cleanup refusal and
retry; released owned focus/monitor views end when their cleanup succeeds.
All calls require exclusive use of the complete live App graph on its video
thread. No source exception handler or shutdown equivalence is implied.

## Evidence and limits

The authority is the pinned original client JAR, SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
`javap -c -p net.minecraft.client.Minecraft` establishes the enclosing input
order at337..1073 and mining/reset caller `a(IZ)V`. Public
[input-field fixtures](../../tests/reference/client/input_fields/README.md)
compare63 original inventory wheel observations. The
[controller fixtures](../../tests/reference/client/controller_tick/README.md)
compare60 original tick/reset observations. Settings stores and connected
input composition have bytecode-derived and authored integration checks;
they are not observations of a complete original `Minecraft.k` execution.

The Windows World-selection profile loads a disposable saved World. The loop
reaches input return1073 and continues into the admitted World/frame segment.
Direct calls
of that input segment on the same live graph then check actual SDK key
press/release, hotbar stores, all four toggles, wheel
selection, stale-wheel rejection and unread keyboard events after a pause
dependency. The [ticking contract](renderer-ticking.md) records the later
paused-frame and unpaused-scheduler dependencies.
The profile preserves save/resource files and completes native cleanup. Focus,
held-click and pass-through-screen routing lack direct new integration checks. These
finite checks do not establish movement, mining, full-frame rendering, extended
play or multiplayer.
