# Active Player tick reference

The optional game profile calls inherited `gs.w_()` on an actual constructed
and World-published `dc`. It uses the supplied unchanged Beta1.7.3 client JAR,
actual `kq`/Minecraft, World/Chunk, Session, Input/settings, Inventory, writer,
notice and statistic owners. The helper defines no replacement game class,
uses no Unsafe allocation and does not replace a virtual implementation.
Authored source fields and actual Chunk arrays are controlled test inputs.

There are 39 exact rows: 34 completed calls and five genuine null-reference
prefixes, including 16 consecutive forward ticks. Ten additional direct
private-body rows compare actual push-out lower/upper occupancy, all directions,
closed neighbors and strict ties. The original calls its genuine private
method reflectively and verifies its false return; the native completion flag
is separate from that discarded source value. The five prefixes check the
actual top Java failure frame. Case22, the native GUI-close dependency, is
excluded. Absolute notice times are bounded independently around the actual
call; all serialized raw state, flags, counters, identities, totals and Entity
Random state compare exactly. There are no numeric tolerances or saved answers.
Existing Entity and World Random owners are reseeded after construction with
explicit input38 on both sides. Default construction entropy, full startup,
outer World/riding admission and full gameplay are outside this profile.

## Native tests without a JDK or game assets

Build the configured test executables against a complete consistent source
cohort. The actual full application layout and reviewed dependencies must match
the linked libraries. `INTEGRATION.md` in the staging handoff names the targets;
the seven runtime files belong to `mc_simulation`.

```text
python -B tests/reference/entity/active_tick/run_native.py --executable <mc_test_player_active_tick> --work-dir <fresh-private-output>
python -B tests/reference/entity/active_tick/run_native.py --executable <mc_test_player_active_allocation> --work-dir <another-fresh-private-output>
python -B tests/reference/entity/active_tick/test_controller.py
python -O -B tests/reference/entity/active_tick/test_controller.py
```

The driver creates small authored translation/map files and fresh worlds.
Those are real resource-file inputs to the actual Registry. It requires a
positive final check-count marker, successful exit, empty stderr, bounded
streams and matching retained hashes. The allocation target compiles the real
new wrapper object with a private test allocator, exercising allocation failure,
repeated retirement and actual WorldAccess callback reentry. All other bodies
are ordinary configured production objects.

The native-only second-corner case loads an actual stored Chunk/Item and clears
bounds from its real Entity-added WorldAccess callback. It checks the first
push-out's retained publication and velocity before failure at the original
second bounds read349, then restores the genuine allocation and retires the
extra Chunk through the normal native lifecycle. It adds no original tick row
or replacement World getter.

## Original Windows profile

The genuine application profile is currently qualified only on Windows x64,
using explicit matching JDK17 java/javac executables and the pinned Windows
LWJGL native. It initializes the real application superclass rather than
substituting an empty Minecraft. The required native directory must contain
`lwjgl64.dll`, SHA256
`86a8e4555d3614e7ed5a24beda921396ceb9e41dbf2508d713a3d26b928862b7`.
The controller rejects other hosts for this profile. Linux native and Math
component evidence does not imply a Linux original-application qualification.

```text
python -B tests/reference/entity/active_tick/compare.py --executable <configured-native-test> --client-jar <b1.7.3.jar> --libraries <pinned-library-directory> --natives <qualified-Windows-native-directory> --java <jdk17/bin/java.exe> --javac <same-jdk17/bin/javac.exe> --source-root <consistent-source-root> --tested-commit <exact-source-label> --work-dir <fresh-private-output>
python -B tests/reference/entity/active_tick/audit_dispatch.py --client-jar <b1.7.3.jar> --source-root <consistent-source-root>
```

The client JAR SHA256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The four required library filenames and full hashes are in `compare.py`:
LWJGL2.9.0, lwjgl_util2.9.0, JInput2.0.5 and JUtils1.0.0. No dependency download
or second native compiler runs inside the controller. It uses the supplied
configured executable.

`source-pins.json` records 45 whole original classes, 844 methods and 837 Code
bodies. The controller audits all of them before running. A separate dispatch
audit resolves ten actual `dc` virtuals through `dc -> gs -> ls -> sn`, pins the
source superclasses and verifies the literal true Player-container body. The
original helper additionally verifies 21 actual runtime class origins. JVM
class-initialization output remains private and rejects Display/OpenAL
initialization. There is no display/audio context in this profile.

Only these original resources are copied from the verified JAR into the fresh
private native input directory: `lang/en_US.lang`, `lang/stats_US.lang` and
`achievement/map.txt`. Their lengths and SHA256 values are pinned and checked
before and after the native run. No original class, resource, world, binary or
captured answer is distributed in this package.

The controller clears ambient Java option variables, requires matching JDK17
tools, uses `-Xint -Xverify:all`, bounds every subprocess's time/stdout/stderr,
and records compiler, JVM and native command evidence. It pins helper sources,
parser, runtime inputs and native executable before/after the run; compiled
helper classes are pinned immediately after compilation and after execution.
Output directories are fresh and all raw captures remain there on failure.

## Separate atan2 component

```text
python -B tests/reference/entity/active_tick/compare_math.py --executable <configured-mc_player_atan2_probe> --java <jdk17/bin/java> --javac <same-jdk17/bin/javac> --work-dir <fresh-private-component-output>
python -B tests/reference/entity/active_tick/audit_math_origin.py --netlib-source <local-e_atan2.c> --jdk17-source <local-JDK17-e_atan2.c>
```

This component is qualified on Windows and Linux. Its 8,676 raw records combine
22 edge values in both operand positions with 8,192 deterministic word pairs.
Actual JDK17 Math and StrictMath results must agree with each other and with
the configured native probe. Signed zero, infinities and distinct NaN payloads
are included. The qualified native fdlibm `x+y` selects x when both inputs are
NaN; the C port makes that choice explicit rather than relying on operand
commutation. Floating exception flags and errno are outside this value-result
scope. This test does not prove a game method or a frontend loop.

The port originates from permissive Netlib fdlibm. Its Sun notice is retained
in the C file and `licenses/fdlibm-player-atan2.txt`. `math-origin.json` pins the
Netlib source and separately audited OpenJDK17 source. The audit verifies the
same normalized kernel after exactly two documented token differences. Raw
upstream source is supplied separately, not copied into this package.
