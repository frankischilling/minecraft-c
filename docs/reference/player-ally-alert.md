# Original Player ally-alert observations

The [packet](../../tests/reference/entity/player_ally_alert/README.md) executes the unchanged complete Player damage method and its protected ally-alert helper. It uses a genuine original App, Session and statistics writer, one genuine World with 25 prepared normal Chunks, real published Players/Mobs and original watcher objects. No game Code is patched or replaced.

`gs.a(sn,int)` clears idle at bytecode 2, checks health, optionally wakes, applies Monster/Arrow difficulty scaling and rejects zero amount. It resolves an Arrow's Living shooter when present. A Living attacker then reaches `gs.a(ls,false)` at 144 before the damage-taken statistic at 152 and inherited Living damage at 158. The healthy original rows return through 161, including actual health subtraction, knockback and hurt sound.

The helper's 215-byte body has these ordered boundaries:

| Position | Actual source behavior |
| --- | --- |
| 0 / 7 / 14 | Creeper and Ghast exclusions; immediate return |
| 28 / 42 / 48 | Target Wolf tame getter, Player-name equality with its owner, owned-target return |
| 57 / 63 | Player target calls the actual false `gs.F()` body and returns |
| 65 / 100 / 112 / 115 | Capture World; pooled unit box at Player position; expand by `(16,4,16)`; actual Class-based Wolf query |
| 139 / 158 / 166 / 181 | Visit source List occurrence; tame flag; current target; owner equality |
| 187 / 193 | Only true tests sitting; false bypasses the getter entirely |
| 202 / 208 / 214 | Unsit, assign target, continue iteration and return |

Each query considers actual Chunk-slice membership and bounds. An already targeting Wolf, foreign owner, untamed Wolf or outlying Wolf is unchanged. False also unsits owned sitting Wolves. A direct null target passes the instanceof guards, runs the query and assigns null to eligible Wolves; it does not dereference the target.

Actual `ud.a(int)` and `ud.c(int)` read the Map/record's Object payload and cast it. The declared `ma` type remains unchanged by the malformed payload profiles. Null watcher, Map or absent record produce their actual first null exceptions. Null Byte payload faults on unboxing; Integer payload produces ClassCastException. A null owner String value is legal and fails a nonnull Player-name equality test. A null Player name faults at the actual equals receiver after the owner getter. Partial earlier Wolf target/dirty effects remain recorded.

The remote input demonstrates caller order: ally alert and statistics finish before inherited Living damage returns false. Null App and null Inventory inputs retain earlier ally effects; the Inventory case also retains the statistic and reached Living hurt prefix. The packet does not claim normal recovery from these source exceptions.

Four independent original VMs agree on 40 selected calls and 57,691 scalars. There are 19 normal returns and 21 first exceptions per VM. The normal rows include nine true damage results, three false damage results and seven direct void helper returns. Normal and optimized portable controls pass 895 stream checks, 4,134 passive events and 1,201 corruption refusals.

Default constructor Random inputs remain independent and are never reseeded. Original raw field readbacks retain unwritten constructor entropy. Each entered source call preserves World and Math streams; successful hurt sound consumes exactly two values from that Player's actual private stream. JDI verifies actual entries, returns, arguments, failures and executing Code. Cross-VM trace comparison validates each run's private pitch before excluding that independent value from equality.

The native comparison observer is separate from the captured original producers. Its selected transport pairs 57,580 scalar positions and retains 111 original Java message/stack positions without native equivalence. Any native pass must come from its own complete guarded process; this reference packet does not establish one by itself. Checked native cleanup follows the observation and does not replay source queries or setters. Native limits, representation failures and cleanup refusals receive no source false-return credit.

The first native forty-row opportunity completes with empty stderr and 11,342 checks but is refused before scalar comparison because its two output writers reorder buffered case markers after all constructor banners. That failure remains retained. Checked per-case and footer flushes correct the observer transport; the exact ordered parser and original expectations stay unchanged.

Subsequent guarded Windows and sanitized Linux opportunities each pass all 40 calls, 19 normal returns and 57,580 shared scalars, with 11,382 fixture checks. The 111 original Java message/stack positions remain unpaired. Linux also passes the focused 11/11 selection in 7.72 seconds with ASan, UBSan, leak detection and halt-on-error.

Windows retains opportunity `ally-alert-comparison-p5_i7eny` and native digest `3bca0fb48811e7079584c3685ddadb15065c3eafcd5ac15acd5e154b6d052c92`. Linux retains `/home/frank/minecraft-damage-wake/build/player-ally-alert-original/ally-alert-comparison-940r_fxe/run.json` and digest `47528d9c3c8f3fa01c67b2b2361f11c7938627cf8eea4cb54445eaabb8bd3116`. These whole-output hashes differ with actual independent constructor entropy and host fields; the comparison equates only the declared selected positions and validates each run's private stream separately. Each native receipt binds its own source/product opportunity.

The frozen original pins already include BP's 50-byte constructor and 18-byte watcher hook. A separate [post-capture archive audit](../../tests/reference/entity/player_ally_alert/ghast-construction-pins.json) records the six-byte `wq(fd)` parent, inherited LS/SN constructors and relevant watcher code. This audit adds no runtime observation. Broader Ghast ticking is outside the helper's early-rejection path and retains its own production frontier.

See the [production ownership notes](../entity/player-ally-alert.md) for caller/retained-child integration. These are prepared CPU components, with no World ticks, loaded encounter, graphical/audio comparison or FPS qualification.
