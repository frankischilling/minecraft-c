# Normal population source map

Authority is the verified Beta 1.7.3 client and dedicated-server bytecode
identified by the [retained reference hashes](biome.md#reference-identity).
The complete paired entry bodies
are client `yf.a(Lcl;II)V` and server `pk.a(Lbl;II)V`, PCs0–2906.
The dedicated binary supplies the base-server body here; native tests using
load-or-create constructors do not establish a dedicated startup route.
Raw bytecode/decompiled text stays private.

The complete body and actual selector declarations were compared before the
[native controller](../world/population/normal.md) was installed. The shared
structure agrees across both sides; class names/descriptors remain side-specific.

| Source interval | Behavior |
| --- | --- |
| 1–113 | Declare-class falling true; current manager/point Biome; two seed reads and shared-Random mixing |
| 126–322 | Optional water lake; optional lava lake with its height/additional gate |
| 344–403 | Eight Dungeon attempts |
| 431–486 | Ten clay attempts, size32 |
| 514–575 | Twenty dirt attempts, size32, Y bound128 |
| 603–664 | Ten gravel attempts, size32, Y bound128 |
| 692–753 | Twenty coal attempts, size16, Y bound128 |
| 781–841 | Twenty iron attempts, size8, Y bound64 |
| 868–928 | Two gold attempts, size8, Y bound32 |
| 956–1016 | Eight redstone attempts, size7, Y bound16 |
| 1043–1103 | One diamond attempt, size7, Y bound16 |
| 1130–1200 | One lapis attempt, size6, two independent nextInt16 height draws |
| 1231–1468 | Actual last-eight-layer noise, density/gate, live Biome comparisons, virtual tree selection/placement |
| 1483–1895 | Biome-dependent flowers, tall grass/metadata and dead bushes |
| 1910–2166 | Separate flower/red/brown mushroom gates |
| 2188–2431 | Ten reeds, optional pumpkin, desert cactus loop |
| 2459–2645 | Fifty water springs and twenty lava springs, with distinct nested height draws |
| 2660–2684 | Current manager, current provider w, actual temperature method, successful w publication |
| 2755–2886 | Snow top query, current w reload, adjusted temperature, air/material predicates, notifying write |
| 2903–2906 | Falling false only on ordinary return |

## Evaluation and ownership constraints

Chunk origins and subsequent integer/long arithmetic wrap as Java operations.
Odd seed factors use signed `/2 *2 +1`; bitwise OR with one changes negative
inputs. The second World seed is read after both factors and coordinate mixing.
Every feature uses the same live provider Random. World calls may recursively
populate and alter that Random; no continuation is saved or restored.

Tree-count noise uses the constructor's last eight terrain layers. Ordered
Biome-static equality tests remain separate live reads. Counts add or overwrite
according to each original group; a cached canonical Biome enum is insufficient.
Base and inherited Biome selectors choose oak/big oak; Rainforest, Forest and
Taiga retain their own draw sequences and declaring bodies. A fresh big-oak
constructor precedes scale1440 and height1463.

Temperature reads the current manager at2660 and current nullable w at2664.
The method returns at2681 before actual w store2684. Snow reloads w at2761 after
each acquiring top query2755; daload2772 must use that current identity.
Height subtraction wraps before conversion. Adjusted temperature precedes height
guards. Material.c2843 uses the first query result; query2861 is fresh and only
then compares the late Material.s2864 identity. Snow uses notifying write2886.

The shared falling cell belongs to actual client gk/server ei. Declaring-class
initialization sets false once. Controller true1 and false2903 have no finally
or saved prior value. Existing feature methods retain their own World flags,
static initialization, constructors, predicates and failure boundaries.

The native source port preserves this complete controller order. The
[fresh World comparison](natural-population.md) checks the seed-23 origin
population against both originals, including complete retained Chunk arrays,
ordered light regions, numerical buffers and provider/cave Random state and
copied continuations. Other schedules, all recursive descendant effects,
extreme coordinates and complete persistence remain separate qualification work.
