# Arrow constructor and heading comparison protocol

This is a finite component comparison of the unchanged client `sl` methods:
`<init>(Lfd;)V`, `<init>(Lfd;DDD)V`, `<init>(Lfd;Lls;)V`, and `a(DDDFF)V`.
There are 40 ordered rows in each of two ordinary and two read-only JDI VMs.
No Arrow tick, publication, NBT, renderer, scheduler, or performance claim is included.

`profiles.tsv` contains 12 signed decimal integer columns, in this order:
`case entry world shooter geometry direction speed spread seed cache reuse fault`.
`entry`: 0 World constructor, 1 position constructor, 2 Living constructor, 3 heading.
`world`: 0 null, 1 World A, 2 World A with `B=true`, 3 distinct World B.
All nonnull shooters belong to World A. `shooter`: 0 null, 1 Skeleton, 2 actual
local Player, 3 Wolf, 4 Ghast. Shooters are prepared once, are not published or
ticked, and receive the actual source pose `c(8.5,64,8.5,30f,10f)` before each
Living row. `fault`: 0 none, 1 null Arrow Random, 2 shooter health zero,
3 Arrow `be=true`. Fault writes occur before the observed method entry.

Position `geometry` selectors supply these exact binary64 triples:

| Selector | x | y | z |
|---|---|---|---|
| 0 | 7 | -3 | 11 |
| 1 | -0 | +0 | -0 |
| 2 | smallest positive subnormal | -smallest positive subnormal | -17.25 |
| 3 | `0x7ff8000000001234` | +Infinity | -Infinity |

Heading direction selectors are 0 `(1,2,3)`, 1 `(0,0,0)`, 2 `(-0,+0,-0)`,
3 `(min-subnormal,-min-subnormal,min-subnormal)`, 4 `(1e300,-1e300,1e300)`,
5 `(NaN,2,3)`, 6 `(1,NaN,3)`, 7 `(1,2,NaN)`, 8 `(+Inf,2,3)`,
9 `(1,-Inf,3)`, 10 `(1,2,+Inf)`. Every NaN input uses binary64
`0x7ff8000000001234`. Speed/spread selectors are 0 `1.5f`/`1f`, 1 `0f`,
2 `-1.5f`/`-1f`, 3 binary32 NaN `0x7fc01234`, 4 positive Infinity.

For a new standalone heading Arrow, first run the genuine World constructor
outside the observed scope, then author `aP=.25,aQ=-.375,aR=.5`,
`aS=11,aT=-12,aU=13,aV=-14`, `j=37,k=75`, and `Random.setSeed(seed)`.
`cache=1` additionally calls its genuine `nextGaussian()` once before entry.
`reuse=16` means reuse row 16's actual Arrow without any field, seed, or cache
reset; it is the sole reuse row. Rows 0..15 do not seed Arrow constructor
entropy or substitute constructor Gaussian returns. World random and shared
Math random are only read, never rebound or seeded from captures.

The row prefix is `ARROW_CONSTRUCTION ` followed by one JSON object:

```
{schema:"ARROW_CONSTRUCTION_RAW_V1",case:int,input:[12 ints],
 arguments:{position:[3 Dhex],direction:[3 Dhex],speed:Fhex,spread:Fhex},
 returned:bool,exception:"NONE"|"java/lang/NullPointerException",
 before:context,after:context,arrow_before:snapshot|null,arrow_after:snapshot|null,
 exception_message:string|null,exception_frames:[strings]}
```

All `Dhex` and `Fhex` values are lower-case raw IEEE words of length 16 and 8.
Unused argument vectors remain the declared selector's values; `input.entry`
defines which arguments actually enter the source method. Context is:
`{counter:int,world_random:random|null,math_random:random|null,shooter:snapshot|null}`.
`counter` is actual source `SN.a`; compare its before/after difference, not
absolute bootstrap IDs. A random is `[seed16,bool,gaussian16]` or null. Preserve
the actual cached value even when the cache flag is false.
`world_random` observes the selected actual Arrow World (null for selector 0,
World B for selector 3). `source_raw` additionally retains actual reflected
SL/SN/shooter instance fields without inventing native counterparts for them.

Snapshot is:

```
{class:"sl"|"fr"|"dc"|"gi"|"bp",id:int,world:"world-a"|"world-b"|null,
 arrow:{a:bool,b:int,c:"shooter"|null,d:int,e:int,f:int,g:int,h:int,i:bool,j:int,k:int}|null,
 position:[aM,aN,aO],motion:[aP,aQ,aR],history:[aJ,aK,aL,bl,bm,bn],
 angles:[aS,aT,aU,aV],dimensions:[bg,bh,bf],bounds:[6 Dhex]|null,
 dead:bool,random:random|null,
 watcher:{map_null:bool,dirty:bool,records:[{key:int,type:int,id:int,
 value:{kind:"Byte"|"Integer"|"String"|"Null",value:int|[UTF16 units]|null},dirty:bool}]}|null}
```

The native row schema is identical. Native-only diagnostic data goes in the
additional `native` object: `{site,heading_site,heading_entered,heading_returned,
gaussian_calls,random_captured,random_before,random_after,gaussian_values}`.
Each site is `{class,method,descriptor,pc}` or null. This extra data is validated
against actual JDI events and within-run arithmetic; it is not a fabricated
source event journal. Root owns the native C emitter.

Watcher payload tags describe the actual object independently from the record's
declared `type`. In particular, genuine Wolf records include Byte16, String17,
and Integer18. Strings are exact UTF-16 code-unit lists (including an empty
list); a null value is `{kind:"Null",value:null}`, not an empty string.
Diagnostic record arrays are sorted by actual key. This is canonical transport,
and makes no claim about the live source HashMap's traversal order.

Ordinary reflection cannot obtain the failed constructor receiver. Therefore
rows 14/15 have ordinary `arrow_after=null`; JDI retains the actual `this` from
SL constructor entry and snapshots its reached fields at the first source
exception. The JDI call record includes `entry_arrow`, `exception_arrow`,
`return_arrow`, `math_initialized_before/after`, actual ENTRY/RETURN/STORE and
EXCEPTION events, and the live source Code hashes. No debugger invocation or
field write is allowed. Native failed prefixes compare to these JDI snapshots,
not to a modeled constructor.

Row 0 runs before App/World bootstrap. Its native fixture may use a separate
minimal owner; only its constructor state, ID increment, absence of IN active
use/Gaussian, and within-run stream invariants are paired. Subsequent bootstrap
and absolute IDs are outside this row's comparison. Row 1 begins after real
World/App/shooter preparation. Shared Math holder preparation is declared
fixture work; it does not initialize source `in`. JDI, not reflective static
reads, proves source `in` initialization and active helper calls.
The native minimal row-0 fixture materializes its actual shared holder through
one public Math.random draw before the observed constructor; the Java fixture
materializes the JDK holder reflectively without that draw. These independent
fixture prefixes and raw initial seeds are unpaired. Both check that the actual
Arrow invocation preserves its prepared stream and leaves source Math cold.

Constructor default entropy and constructor heading's derived motion/angles
are retained raw but unpaired across independent VMs/native processes. For
each run, actual Gaussian receipts verify cache/stream evolution and source
arithmetic. Seeded standalone headings pair full state and stream outcomes.
Java exception messages/stack frames and native policy/admission diagnostics
are retained unpaired. No broad epsilon, NaN rewriting, source replay, or
constructor entropy normalization is permitted.
