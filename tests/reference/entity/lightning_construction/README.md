# Original Lightning constructor prefixes

This CPU-only package executes unchanged client `c(World,DDD)` through actual
YU and Entity construction. It runs five calls in an ordinary VM and another
five under read-only JDI: difficulty 0, difficulty 1, difficulty 2 with the
radius outside the vertical World range above and below, and a null World.
The first four return. The null World throws its actual NullPointerException
at C52 after pose, nextLong and nextInt(3)+1 stores.

```sh
python -B tests/reference/entity/lightning_construction/capture_reference.py \
  --client-jar /private/b1.7.3.jar --output .local/lightning-construction-new
```

Use matching JDK 17 `java` and `javac`; the wrapper also accepts their paths.
The output directory must be new. It pins the original JAR, complete 176-class
initialized closure, all method Code identities, authored sources and three
compiled helpers. `ConstructorJson.java` is shared authored transport from the
Mob-construction package. Classpaths contain only those helpers and the original
JAR. JDI independently checks seven live source bodies and 84 checkpoints.
Original resources, outputs and classes remain in the ignored output directory.

The target constructs a genuine World and prepares the ordinary registry through
an actual Session. It initializes the ordinary Math holder before active cases
so both VMs can observe its unchanged seed. It assigns only the World difficulty
argument context between independent calls and its own observer fields. It
never assigns an Entity field, source counter, watcher, RNG seed or clock.
The observer invokes no target method and changes no target field or class.

Each JDI call records the actual SN private-Random seed and actual JDK
seedUniquifier/nanoTime method returns. Python verifies their constructor
transform and independently replays nextLong, including its signed low int,
and bounded nextInt with rejection. All reached checkpoints retain the same
Math seed. Independent VMs have independent entropy; their random-derived
fields are validated against their own observations rather than compared as
equal. Native fresh-profile clock controls remain separate evidence.

An ordinary failed allocation is unreachable after its constructor throws.
Its row therefore records the source exception with null observation fields.
Read-only JDI observes that same failed object's completed prefix at C52 and
the actual ExceptionEvent at C52. No replacement failed object is created.

`observations.json` retains ordinary/debugger rows, five actual entropy profiles,
84 checkpoints, ten clock returns and the source exception. Every capture
checks input and helper fingerprints before and after execution. This finite
scope covers constructor and no-fire prefixes. Fire placement, notifying
mutation failures, weather publication, Lightning ticks and rendering require
separate evidence; this package does not claim those paths are complete.
