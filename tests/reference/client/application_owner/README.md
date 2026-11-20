# Minecraft class initializer and kg CPU profile

These authored sources are uncompiled and unrun. They provide a proposed
normal-profile comparison for actual Minecraft class activation and real `kg`
workers. They do not qualify the instance constructor, startup, run loop, World
entry or a playable session.

`source-pins.json` binds the complete class/Code identities read offline from the
unchanged supplied client. Minecraft has SHA256
`db99df969e5f30d01ad4424a4d63d3b1f4a11832055dd291e65d476cfa4c38b3`;
`kg` has SHA256
`4c59482427c8a2f649b0ab89069cb6a058249ca2aa764bc6767872c26ea68889`.
The original client JAR has SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Source authority includes each entire class, not a bytecode excerpt.

`ApplicationOwnerObservation.java` initializes the unchanged Minecraft class
and reads its real `b`, `E`, `F`, `G`, `H`, `a` and `af` static slots. It checks all
entries of the three arrays for zero and records their lengths and distinct
history-array identities. It allocates the unchanged original concrete `kq`, whose direct superclass
is abstract Minecraft, through the JDK17 Unsafe boundary. The inherited fields
have real JVM defaults. Neither instance constructor is executed; no `kq`
virtual/frame method is called. The complete original `kq` class/Code is also
pinned, and it has no static initializer or shadow J/o field. It then invokes the unchanged `kg` constructor twice: once with
default-false `J`, and once with true `J` followed by source stop plus interrupt.
Both started threads must terminate and join. A `finally` path interrupts and
joins any retained worker before the observer exits.

The prepared-receiver profile is explicit. It does not claim the genuine
Minecraft instance constructor ran, does not activate its other constructor
children and does not supply a shadow Minecraft class or rewritten `kg` body.
The observation needs the actual supplied client and explicitly pinned support
JARs, with JDK17 `java` and `javac`. It must not need a display, OpenGL context or
LWJGL native library. Future execution must verify actual inputs and bound
captures; no reference execution has been performed for this proposal.

The C executable's `original` profile is intended to produce exactly these
five records, with only LF/CRLF transport spelling admitted:

```text
CLASS arrays 10485760 512 512 1 1 1 1
CLASS fields 0 0 1 1
THREAD zero 1 1 1 0
THREAD live 1 1 1 1
THREAD source-stop 1 0
```

The three array flags mean every entry is zero. The fourth flag means `E` and
`F` are distinct. The two static-field flags mean actual `a` and `af` are null.
Thread rows report daemon policy, supplied name identity, terminated/sleeping
state and the actual `J` value. The final row reports termination and `J`.
No original/native state, value, identity relation or game field may be
rewritten to obtain agreement.

The C contract executable separately authors allocation/byte-limit prefixes,
native App registration and holds, native spurious wakes, explicit interrupt
handling and cancellation that preserves source flags. Those controls are
native policy/ownership checks. They are not captured original OOM behavior or
evidence that general JVM Thread semantics have been reproduced.

Before qualification, verify the payload/source preimages, exact formatter,
new normal and optimized controller admission checks, all native inputs and
all configured products. Then build every current target and run the existing
suite plus this test on Windows/MSVC and sanitized Linux/Clang/GCC. Fresh
original execution is required on both actual hosts. Header layout changes
require coherent rebuilding of every App-state consumer. Native handle cleanup,
default Windows stack size, headless/client-disabled builds and unchanged
frontend behavior remain required. No CMake graph, compiler, JVM, controller or
native process has been run for these sources.
