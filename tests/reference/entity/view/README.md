# Original LivingEntity view observations

`observe.py` compiles only the authored `ViewObservation.java` glue against a
caller-supplied original client JAR. It checks the complete JAR SHA-256 before
running. No original Java body, JAR, library or asset is distributed here.

The observer's concrete subclass calls the original `ls(NULL)` constructor and
inherits `ls.e(F)`, `ls.f(F)` and `ls.ac()` unchanged. It assigns authored public
position/rotation fields and resets the real BT pool once before each case.
All coordinate, interpolation, lookup-table and pool methods execute from the
original JAR under `-Xint`; no Unsafe allocation or replacement method is used.

The 192 rows cover exact1 and adjacent float partials, zero/signed zero,
extrapolation, infinity, quiet/signaling NaN, extreme/subnormal coordinates,
raw angle interpolation and the current-look wrapper. Each row contains:

`id partial currentX currentY currentZ previousX previousY previousZ yaw pitch
previousYaw previousPitch positionXYZ lookXYZ currentLookXYZ`

Float inputs use 8 hexadecimal raw bits; double inputs/outputs use16. IDs are
decimal. Pooled vectors remain distinct through the three calls in each case.
The pool reset is an explicit observer caller action, not behavior inserted
inside any view method.

Run with a fresh caller-owned work directory and Temurin17. The final directory
must not exist; the runner creates it atomically and rejects an existing path
before compiling or launching Java. This keeps stale classes from shadowing
the pinned original JAR.

```text
python -B tests/reference/entity/view/observe.py --jar /owned/b1.7.3.jar --work-dir /owned/view-observation --java /jdk17/bin/java --javac /jdk17/bin/javac
```

`validation.json` records the measured Temurin17.0.17+10 commands and hashes.
Pinned JAR SHA-256:
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The frozen `vectors.tsv` SHA-256 is
`03d3bb0bb5e24c1f69ff2f26ab3909d10c36d46069306983401100c58da54897`.

Relevant source PCs: LS.e pooled factory18/83; LS.f lookup17/122 and pooled
factory76/176; LS.ac invokes f(1)2/return5. The observation does not qualify
World ray traversal, Player construction, native failure parity or the whole
PX picker. Ordinary native tests consume frozen observations and require no
Java runtime or original JAR. Native failure/identity controls are authored
contracts, distinct from these primary-binary numeric observations.
