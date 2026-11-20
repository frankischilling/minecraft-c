# Recursive gravel observation

This optional JDK 17 observer calls the unchanged original client JAR on a copy
of the authored native falling fixture. It uses the genuine `mx` and `fd`
constructors, four actual loaded Chunks, the original gravel Block and `gk.b`.
The supplied JAR stays in an isolated loader; its whole-file hash and the
reached classes' archive origins are checked. The helper contains reflection
and observation code only. No original classes, resources or save data are
distributed here.

The authored input has stone at Y4 and gravel at `(20,6..80,20)`, with air at
Y5. Its four Chunks cover X/Z0..31 and are marked terrain-populated. The existing
native [falling test](../../../world/runtime/test_falling.c) writes these inputs
before loading them; its [driver](../../../tools/world/test_falling.py) retains
the resulting input regions. Use `world0` from a fresh successful capture.

Compile and run separately from normal builds and CTest:

```powershell
& $Javac -d $HelperClasses tests/reference/world/falling/FallingRecursiveObservation.java
# Copy capture-.../world-.../world0 to this fresh owned path first.
Copy-Item -LiteralPath $AuthoredWorld0 -Destination $CopiedWorld -Recurse
& $Java -Xverify:all -cp $HelperClasses FallingRecursiveObservation $ClientJar $CopiedWorld
```

Use explicit JDK 17 `java`/`javac`, an empty helper output directory, the supplied
client JAR and a fresh private copy of the authored World directory. The actual
SaveHandler constructor writes `session.lock` in that copy. Keep original saves
and earlier captures out of the observer's writable path. No increased thread
stack, World subclass, replacement callback or substituted game owner is used.

After genuine construction/loading, the observer temporarily sets the same two
explicit fixture inputs as the native control: `fd.a=true` for immediate ticks
and `gk.a=true` for immediate falling. It restores both after the actual neighbor
call and reports the final gravel heights, `qy.f` sequence delta, F/E counts and
the same World's serialized Random state. It records the result independently;
it does not compute the expected survivor count.

The fresh pinned client run on JDK `17.0.17+10` returned **38 gravel Blocks at
Y5..42 and 150 scheduled identities**, with F/E and World Random unchanged.
The native control reproduces that result on both selected native sides.
[validation.json](validation.json) records this bounded observation's commands,
source/result hashes and runtime. The normal native test remains an authored
contract and does not launch Java or compare an original process on each run.

The relevant original client source order was checked with `javap -p -c -s`:

- `gk.b` invokes `fd.c(IIIII)` at13. With `fd.a=true`, scheduling invokes the
  actual current Block tick synchronously at142.
- `gk.a` invokes private `gk.h` at6. That body performs notifying removal79,
  then resumes its original local Y at predicate90 and decrements only at100.
- The synchronous World neighbor order reaches the above Block at41 before
  removal79 returns. Descendants can refill an ancestor's original Y cell.
- Private placement119 discards its ordinary setter result at122. The actual
  Chunk setter returns false at the equal-ID gate before source mutation;
  those resumed ancestors do not introduce another Block or notification.

The result qualifies this one authored recursive client scenario. It does not
establish whole falling parity, arbitrary recursion, native/JVM allocation or
stack-failure timing, later FallingSand movement, collision, placement or drops.
