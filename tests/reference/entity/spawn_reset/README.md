# Spawn/reset reference

The helper executes ordinary `gs.t_()` and the direct inherited `sn.t_()` body
on the same genuine constructed `dc`. Its App, Session, World, Chunk, Registry
and Entity classes come from the supplied unchanged client JAR. It defines no
game replacement class and uses no Unsafe allocation, proxy World, transformed
class or replacement virtual body.

The direct base call uses the legal JDK17 private lookup for `sn`, followed by
`findSpecial(sn,"t_",()V,sn)`. The helper checks the resulting direct handle's
`REF_invokeSpecial` kind and declaring class. Completed base calls must leave
health, death, size and eye height untouched; completed wrapper calls must
apply their actual stores. This independently distinguishes the two entries.
The wrapper's size virtual resolves through `ls.b(FF)` (`sn.b(FF)` at PC3),
and position virtual resolves to `sn.e(DDD)`.

`scenarios.json` defines the controlled source fields, real Chunk-array cells,
record layout and scenario IDs. There are 42 rows: 21 scenarios for each body,
with 34 complete calls and eight actual null-reference prefixes. Raw position,
velocity, previous positions, permanent bounds, size/eye height, angles,
health/death, nullable references and Entity/World Random states are retained.
Inputs include air, floor, a tall solid column, slab, fence, bed, null World,
zero/negative/NaN/subnormal Y, NaN X and delayed null bounds. Null bounds are
prepared reflectively on the actual final instance field and restored before
the next operation. These deliberate source-field prefixes are not ordinary
gameplay-state claims.

The four interrupted scenarios retain actual top frames on both bodies:

| Scenario | Frame | Source point |
|---|---|---|
| 8 | `sn.e` | Permanent bounds invocation86, under `sn.t_` position call30 |
| 13 | `fd.a` | Actual collision List `K.clear`4, under `sn.t_` World call42 |
| 14 | `fd.f` | Actual provider presence invocation6, under the real collision query |
| 15 | `fd.b` | Actual nearby List `R.clear`4, after the collision query's Block work |

All temporary null fields are restored in `finally`. Existing Random owners
are reseeded with explicit input38 after construction. Constructor entropy,
World ticking, controller setup, GUI behavior, device input and full gameplay
are outside this profile.

## Capture and comparison

Run `capture_reference.py` with the verified JAR, the four pinned dependency
JARs, qualified Windows LWJGL native directory and a fresh output directory.
It requires matching JDK17 tools, `-Xint -Xverify:all`, successful bounded
captures and empty stderr. The helper verifies17 actual runtime class origins.
`source-pins.json` records17 whole original classes,673 methods and668 Code
bodies; the full client JAR hash is also checked. The controller checks compiled
helper and source/runtime hashes before and after execution and rejects
Display/OpenAL initialization. It copies only the three pinned translation/map
resources for the separate native comparison input.

```text
python -B capture_reference.py --client-jar <b1.7.3.jar> --libraries <library-directory> --natives <Windows-native-directory> --output <fresh-private-directory>
python -B capture_reference.py --client-jar <b1.7.3.jar> --libraries <library-directory> --natives <Windows-native-directory> --native-stdout <output>/native.stdout --native-report <output>/run.json --output <another-fresh-private-directory>
python -B test_reference.py
python -O -B test_reference.py
```

Native comparison requires exact equality of all42 rows. The parser checks
their body/scenario order, complete/failure partition, actual failure frames,
field counts and hexadecimal widths. The tests use generated records without
embedding original numeric answers. Original execution is qualified on Windows
x64; sanitized Linux native comparison to retained Windows rows has a separate
cross-host scope.

`qualified-original-final/capture.json` records the successful original run.
Its `rows.txt` is private observed evidence, not a distributed expected-answer
fixture. Earlier `initial*` and `qualified-original` directories retain the
development captures and are excluded from the intended source payload.


Native output comparison requires a separate completed-process receipt. Run
`tests/reference/entity/spawn_reset/run_native.py` with the configured executable
and the appropriate `--mode spawn-reference` or `--mode position-probe`. Supply
both `--native-stdout <output>/native.stdout` and
`--native-report <output>/run.json` to the original controller. The controller
checks successful exit, completed bounded pipe capture, empty stderr, stream
hashes, the unchanged executable hash, mode and terminal check-count marker
before comparing numerical records. Numeric rows alone are insufficient.
