# Position arithmetic reference

`PositionObservation.java` invokes the unchanged inherited `sn.e(DDD)` method
on a genuine constructed `dc`. The App, Session, World, Chunk and Registry
classes are actual client JAR owners. The helper defines no replacement game
class, overrides no virtual and uses no Unsafe or transformed classes.

`vectors.json` records36 explicit raw-bit input vectors. `vectors.txt` is the
identical input order in the probe's simple text format; the controller checks
the two representations agree before execution. Each vector prepares actual
width/height through the real `ls.b(FF)` wrapper and actual eye-height/y-size
fields, then calls `player.e(x,y,z)`. The output is:

```text
POSITION <scenario> <x> <y> <z> <minX> <minY> <minZ> <maxX> <maxY> <maxZ>
```

Each value is an exact16-digit lowercase double bit word. Inputs cover ordinary
numbers, signed zero, positive/negative subnormals, distinct quiet/signaling
NaNs in coordinates and float fields, two/three-NaN argument combinations,
maximum finite values, signed infinities and cancellation. Position is the
only game operation after input preparation. No World query, rendering,
controller action or gameplay tick is substituted.

`capture_position.py` pins the supplied full JAR hash,12 original runtime
class origins,600 methods and595 Code bodies, JDK17 tools, four library JARs
and the qualified Windows LWJGL native. It runs `-Xint -Xverify:all`, bounds
subprocess captures, requires empty stderr, checks all helper/source/runtime
hashes before and after execution and rejects Display/OpenAL initialization.
The controller also verifies that `dc`, `gs` and `ls` declare no `(DDD)V`
override of `e`. Optional paired `--native-stdout` and `--native-report` require exact equality of all36
rows against a configured native capture.

```text
python -B capture_position.py --client-jar <b1.7.3.jar> --libraries <library-directory> --natives <Windows-native-directory> --output <fresh-private-directory>
python -B test_position.py
python -O -B test_position.py
```

`qualified-original/capture.json` and `rows.txt` retain the successful Windows
original run. This receipt establishes the original results only. Comparing
both the retained old helper and the new shared helper remains necessary to
distinguish extraction from any numerical correction. These private observed
rows are not distributed expected answers. The original class/method metadata and Java observation body remain unchanged.


Native output comparison requires a separate completed-process receipt. Run
`tests/reference/entity/spawn_reset/run_native.py` with the configured executable
and the appropriate `--mode spawn-reference` or `--mode position-probe`. Supply
both `--native-stdout <output>/native.stdout` and
`--native-report <output>/run.json` to the original controller. The controller
checks successful exit, completed bounded pipe capture, empty stderr, stream
hashes, the unchanged executable hash, mode and terminal check-count marker
before comparing numerical records. Numeric rows alone are insufficient.
