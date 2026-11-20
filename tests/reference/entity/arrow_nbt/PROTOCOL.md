# Arrow NBT comparison protocol

This finite packet enters unchanged client SL own read/write hooks, complete SN
parent read/write/Boolean-save methods, and the JC Compound factory. It does not
tick, publish, render, play a sound, or measure performance. There are 48 calls
in each of two ordinary and two passive JDI fresh VMs. Rows 0..43 form the
representable comparison domain. Rows 44..47 retain four separate source-only
Map/setter failures. Source factory failure snapshots remain source-only where
the native caller has no successful returned Entity.

`profiles.tsv` contains exactly five signed-decimal integer columns:
`case entry mode fault world`. Entry 0 is `SL.a(NU)V`, 1 `SL.b(NU)V`, 2
`SN.e(NU)V`, 3 `SN.d(NU)V`, 4 `SN.c(NU)Z`, and 5 `JC.a(NU,FD)SN`. World 0
is null; 1 is genuine World A. The fixture bootstraps an actual App, statistics
session, two Worlds, and actual Skeleton using the unchanged, separately pinned
Arrow constructor producer as support. Every existing Arrow uses its genuine
World-only constructor before observation. Its current shooter is that actual
Skeleton, j=37 and k=75. These fields and actual Random/cache are observed
before and after every invocation. No captured entropy is transplanted or seeded.

All existing Arrows receive actual pose `(7.25,70.5,-3.75,11.25f,-12.5f)`,
motion `(.25,-.375,.5)`, fall 2.25f, fire 65537, air -65537, onGround true,
and y_size 0 before entry. Own default fields `(d,e,f,g,h,b)` are
`(101,-202,303,404,505,222)` and both flags true. Field authoring happens
outside the observed source entry. Shooter identity is never inferred from NBT.

Rows 0..6 read own values: normal signed coordinates `(17,-19,32767)`,
minimum `(-32768,-1,0)`, maximum `(32767,0,1)`, negative bytes/flags,
Byte2 flags, all missing, and missing xTile. Normal own bytes are inTile=-128,
inData=-1, shake=-2, inGround=1, player=1. Negative mode sets inTile,
inGround and player to -1; Byte2 mode changes only both flags to 2.
Rows 7..14 replace each own tag in source order with an Int tag; row15 uses
a null Compound. Numeric classes remain distinct. Missing tags are absent,
not named-null values. Each read applies its own actual successful prefix.

Rows16..23 write actual own fields. Modes 0..3 use these exact six int tuples:

```
101 -202 303 404 505 222
65535 65536 -65537 256 257 -1
-65536 -65535 65537 -256 -257 255
-2147483648 2147483647 -2147483648 2147483647 -2147483648 2147483647
```

Modes4..7 use `(7,8,9,10,11,12)` and all four flag combinations: i is mode
bit1 and a is mode bit0. Source destinations start with the one unknown Int
`retained_unknown=1234567`; source mutation preserves it. The native independent
result writer compares the eight generated semantic names only. It does not
claim Java mutable destination, allocation-failure prefix, or Map-order parity.

Full parent inputs contain Pos `(8.5,64.9375,-8.25)` Doubles, Motion
`(.1,.2,.3)` Doubles, Rotation `(450,-450)` Floats, FallDistance3.5f,
Fire Short-19, Air Short298, OnGround Byte1, and normal own fields. Rows24..33
are: normal; wrong named Rotation Byte7; missing Rotation; Motion with only
one Double; Motion second element Float; Motion second element explicit null;
Pos first element Float; late player Short; Motion `(+Inf,NaN,-0)` with exact
NaN64 `7ff8000000001234` and Rotation `(810,-810)`; Motion
`(-10.000000000000002,10,-10)` and Rotation `(-0,NaN)` with exact NaN32
`7fc01234`. Wrong and missing initial Lists have different prefixes. Actual
List shape/type and null elements remain represented separately.
Rows34/35 write the complete parent; row35 changes only y_size to .3125f.

Rows36..38 are Boolean saves: alive World A, dead World A, alive null World.
The dead save returns false and leaves the actual destination unchanged.
Rows39..43 are factory calls: Arrow World A, Arrow null World, Arrow with all
own tags absent, Arrow with malformed player Short, and unknown name
`NoSuchArrowType`. Their inputs include full parent tags and id. Constructor
default ID/entropy is independent. The malformed reader executes outside JC's
constructor catch. Unknown-name diagnostics remain retained raw.

Rows44..47 are source-only: xTile Map-present-null read; read with a null
Compound Map; write to null Compound; write with a null Compound Map. These
are never paired with a native missing tag, immutable-output failure or budget
refusal. Null Java StringTag payload is another unrepresented shape and is
outside these rows. The unchanged source is never rewritten to skip its fault.

Each stdout row begins `ARROW_NBT ` and contains one closed JSON object:

```
{schema:"ARROW_NBT_RAW_V1",case:int,input:[5 ints],domain:"paired"|"source-only",
 returned:bool,exception:string,saved:bool|null,before:context,after:context,
 arrow_before:snapshot|null,arrow_after:snapshot|null,result:snapshot|null,
 tag_before:tag|null,tag_after:tag|null,exception_message:string|null,
 exception_frames:[strings],source_raw?:object,native?:object}
```

Context and `snapshot.entity` follow the unchanged Arrow constructor protocol.
Snapshot additionally contains base fall/fire/air/on_ground/y_size values,
same permanent bounds identity and the same original bounds' current values.
Float and Double values are raw lowercase hexadecimal IEEE words. Random/cache
is retained raw and checked unchanged within an existing-Arrow invocation;
default entropy and absolute IDs are unpaired across independent processes.
Factory ID increment is paired. Every source-only field remains raw in the
finite packet rather than being replaced with a modeled native value.

Tag kinds use NBT numeric IDs. Scalar value is signed integer, `{F:8hex}`,
`{D:16hex}`, `{J:16hex}` or `{utf16:[exact units]}`. Compound is
`{kind:10,map_null:bool,values:{name:tag},source_order:[{key,tag}]}`; List is
`{kind:9,element_type:int,values:[tag|null]}`. Compound source_order is the
actual source Map traversal, retained unpaired. Comparison canonicalizes only
unique Compound names; it preserves all values, tag kinds, list order and raw
bits. Writer projection selects generated semantic keys and records its
independent-result contract explicitly; source unknown retention is checked
within-run and retained raw.

Passive JDI does not invoke methods, set fields, replace results or execute
native code. It records genuine source entries, returns, writes, first thrown
exceptions, per-call receiver snapshots and live Code pins. Failed factory
receivers are held only for observation, then released. The journal is source
order evidence, not a fabricated native event stream. No whole-game, loaded
Chunk, renderer, audiovisual, timing or FPS equality follows from this packet.
