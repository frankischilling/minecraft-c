# Pause screen construction and initialization

The fixture contains 68 numeric observations from the pinned Beta 1.7.3
client's unchanged `oz` constructor, `b()V` initialization and `a()V` tick.
The observer constructs real OZ/DA and Button instances. App and World are
actual original classes allocated without their constructors; only the App
World slot and prepared World.B are authored. No original class, asset or
decompiled source is included.

Cases cover a null App prefix, a single-player label and a remote Disconnect
rewrite. Four geometry profiles include ordinary GUI sizes, negative sizes and
signed integer limits. Rows retain the source save/tick counters, completion,
Button order, coordinates, dimensions, flags and UTF16 label units. A tick after
each initialization checks that the independent tick counter advances while
the save-pass counter remains unchanged. App/World construction and remote
network behavior are outside this prepared-operand scope.

[Source pins](source-pins.json) identify the OZ bodies, DA constructor and
translation wrapper. The original client JAR SHA256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
To recapture using an installed JDK17 and locally supplied pinned SDK metadata:

```text
python -B tests/reference/client/pause_screen/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-pause-capture
```

The runner requires a fresh directory, bounds subprocess duration/output and
verifies original/runtime/helper hashes before and after execution. Its observer
denies file writes, network access, listeners and native-library links. A
successful original capture records zero network/native attempts.

The public `client.pause_screen` replay constructs native source owners, uses
two authored translation properties and compares every row exactly. Its direct
pause tick API invokes the same locked OZ body as virtual screen dispatch and
does not require an audio/event target. Native controls also exercise rejected
RAW/active ticks, constructor allocation failure, tick integer wrap,
reinitialization with both original List clears, retained old Buttons and
external screen holds during checked retirement. FU class state remains cold;
opening OZ does not construct the main menu's Random or splash state.

These component observations do not execute the Minecraft pause route,
display/focus handling, actions, World saving or native drawing. Those require
separate connected-graph checks. The source drawing implementation calls the
actual incremental save body before its saving label, title and Buttons.
Options, Achievements, Statistics, remote disconnect and World detach retain
their reached dependencies; they are not completed by this fixture.
