# Original Item appearance scalars

Two fresh ordinary OpenJDK 17 VMs executed unchanged Beta 1.7.3 methods on all
202 genuine registered Items (38 receiver classes), using the 20 signed damage
operands in `operands.txt`. Actual virtual calls observe Item icon `a(I)I`,
actual newly constructed `IZ.b()I` icon, color `f(I)I`, full-3D `b()Z`, and
rotation `c()Z`. Reflection records the declaring owner of each Item method;
CLASS rows include the actual registered Item's signed `bh` field.

The helper performs genuine original Block-before-Item initialization. All
game classes, receivers, constructors, and method bodies come from the pinned
original JAR. It does not replace or recompile original methods. Leaves color
invokes the real JH child, including its actual class initializer; there is no
substitute color table or bypass of the initializer's allocation.

The complete text protocol is:

```text
CLASS ID runtimeClass iconOwner colorOwner full3dOwner rotateOwner bh
APPEARANCE ID damage directIcon actualIZIcon color full3d rotate
BM ID bm damage directIcon actualIZIcon
COMPLETE 202 38 4040 64
```

Fields are separated by one tab. Integers are canonical signed decimal;
Booleans are integer 0 or 1. All 202 ascending-ID CLASS rows precede the 4,040
APPEARANCE rows in ID/declared-damage order. The 64 BM rows follow in Item
ID 6 then 18 order, each with the eight `bm-operands.txt` values and damage
0 through 3. Every BM row captures live `UU.z` or `UU.L`, saves its actual
instance `bm`, stores the declared value, calls both original icon entries,
then restores the saved field in `finally` before the next row. Sapling damage
0/3 reaches its inherited texture field read; 1/2 also observes its literal
branches. Leaves includes the original wrapping `bm + 80` boundary. No static
final field, Source clock, RNG, World, or input state is altered.

`ordinary-0.stdout` and `ordinary-1.stdout` preserve identical exact captured
CRLF bytes, including the original bootstrap and final guard. Their `-text`
attributes preserve the pins, with the repository's `cr-at-eol` whitespace
policy. `original.stdout` is the declared ASCII CLASS/APPEARANCE/BM/COMPLETE
line selection with LF endings: all 4,307 selected lines, without subsets or
value normalization. Authored helper, inputs, and scripts were LF before the
capture freeze. `declared-inputs.json` also has explicit `-text` protection.

`source-pins.json` records class hashes for all receivers plus the declaring/
texture/wrapper/color children, and selected method Code hashes. It includes
JH initialization/constants and BK/HE texture children. No original Code
bytes, classes, JAR, assets, or decompiled bodies are published. Portable
`capture-provenance.json` records tool/SDK/input pins and the identical exact
before/after maps. Helper class hashes were frozen before both fresh VMs.
Compilation and both runs succeeded; there were no failed attempts. Run
stderr contains only the JDK SecurityManager deprecation warning; its exact
private raw hashes are retained, without publishing its private helper path.
The raw freeze/argv, helper products, stderr, and all outcomes remain private.

Audit and finite corruption controls:

```text
python tests/reference/item/appearance/packet.py
python -m unittest discover -s tests/reference/item/appearance -p test_packet.py
```

`packet.audit()` returns a summary; `reference_lines()` audits and returns the
entire literal comparison stream. Repeat with pinned privately supplied inputs:

```text
python tests/reference/item/appearance/capture_reference.py --jar <original.jar> --libraries <libraries> --jdk <jdk-home> --output <new-private-directory>
```

These are finite original scalar observations and declared instance-field
mutations. Native parity, rendered pixels, gameplay, callbacks, World behavior,
timing, and unrestricted rendering require separate qualification.
