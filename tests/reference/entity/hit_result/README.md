# Hit-result geometry observations

`GeometryProbe.java` supplies authored coordinates to unchanged original client
BT/EQ/VF and server BA/CZ/NH classes. Reflection selects their real factories,
distance, plane, point and intercept methods; it reads the actual pool cursor.
It does not replace a method body, forge an object or load a World.

The 942 rows cover six face directions, corners, inside starts, ordinary misses,
adjacent values around the squared-delta threshold, signed zero, small/large
finite coordinates and three input-identity modes. Those modes use standalone
vectors, pooled vectors followed by a real cursor reset, and one object in both
operand positions. Client reset is BT.b(); server reset is BA.a(). Client BT.a()
clears the pool and would test a different operation.

Both pinned JARs produced byte-identical `finite.tsv` under Temurin17.0.17+10
with `-Xint`. The file contains authored numerical inputs and observed numerical
outputs, not original classes or decompiled code. Hashes and the finite scope
are recorded in [validation.json](validation.json).

Each row contains: mode; start XYZ; end XYZ; bounds min/max; three plane
coordinates; distance; squared distance; squared distance to coordinates;
point-inside flag; X/Y/Z plane results; bounds hit; and four pool cursors.
Comma groups contain raw binary64 hex words. `-` means a normal null result;
a hit starts with its block-face integer. The native `entity.hit_result` test
compares the same fixture against both source namespaces and checks their pool
effects. Native constructor, quota, ownership and null-receiver controls are
separate from these original observations.

To regenerate privately, compile the observer into a separate directory and
run it with that directory and the corresponding verified JAR on the classpath:

```text
javac -d <private-output> GeometryProbe.java
java -Xint -cp <private-output>;<client-jar> GeometryProbe client
java -Xint -cp <private-output>;<server-jar> GeometryProbe server
```

Use `:` as the classpath separator on Linux. The native distance arithmetic
uses the existing initialized float-root kernel; this fixture does not establish
native lazy MathHelper initialization timing, general NaN payload parity,
World/PX picking, or a rendered client frame.
