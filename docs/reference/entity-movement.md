# Entity movement source map

The [verified original binaries](binaries.md) remain the behavioral authority.
The [native contract](../entity/movement.md) describes the current Item/Falling
implementation and native policies. Mappings provide aliases, not behavior.

## Complete paired bodies

The complete base movement bodies are client `sn.b(DDD)V` and server
`lq.a(DDD)V`. Each has 1,016 instructions, a 2,029-byte Code extent and returns
at PC88 or PC2028. Every opcode, branch, numeric constant and non-pool operand
agrees after the explicit side-specific field/method reference map. Actual
Item `hl/ez` and FallingSand `ju/gl` inherit this movement owner; their own
`n()Z` overrides return false. No walking-positive kind is supplied here.

| Source phase | Important PCs |
| --- | --- |
| No-clip | Same-box translation PC15; position PCs39/62/85; return PC88 |
| Ordinary prefix | Smoothing PC97; clear web PC121; zero velocities PCs146/151/156; pooled copy PC173 |
| Sneak ledges | Real sneak PC186; X transform/query PCs229/232; Z PCs310/313 |
| Main collision | Sweep/query PCs391/394; Y clip/translate PCs431/448; X PCs533/550; Z PCs608/627 |
| Step trial | Flat pooled copy PC724; initial-box assign PC735; sweep/query PCs751/754; Y PCs791/808, X PCs865/882, Z PCs940/959; downward PCs1048/1065 |
| Step choice and final state | Compare PC1089; restore PC1109; smoothing PC1155; position PCs1178/1201/1224; flags PCs1248-1310; fall PC1319 |
| Contact | Concrete n at PC1378 skips walking for current kinds; loaded area PC1792; ID PC1841; Block array/index PCs1851/1856; contact PC1868 |
| Environment | Wet PC1890; contract PC1912; burning PC1915; fire damage PC1923; fizz literal PC1991; Random PCs2001/2008; sound PC2016; fire reset PC2025; return PC2028 |

| Dependency | Client / server | Behavior |
| --- | --- | --- |
| Watcher Byte | ud.a(I)B / mo.a(I)B | Map.get8; record cast13/value16; Byte cast19/unbox22 |
| Fall-state | sn.a(DZ)V / lq.a(DZ)V | Callback18 before reset23; negative displacement accumulation44 |
| Float fall | sn.b(F)V / lq.a(F)V | Passenger recursive virtual12 only when nonnull |
| Fire helper | sn.a(I)V / lq.a(I)V; Item hl/ez.a(I)V | Base immunity then damage10; Item override calls damage3 |
| Damage | sn/lq.a(Entity,I)Z; hl/ez override | Base attacked-only false; Item wrapped health/dead prefix then false |
| Wet | sn.af()Z / lq.ac()Z | Actual in-water short return; coordinate floors then World rain32 |
| Area loaded | fd/dj.a(IIIIII)Z | Y gates before chunk shifts; private presence69/provider6 |
| Rain at position | fd.t(III)Z / dj.s(III)Z | Raining1; sky13; top24; manager34; point39; Biome c46/d56 |
| Burning volume | fd.c(eq)Z / dj.d(cz)Z | Ordered six floors; area69; ID113; Fire/Lava statics120/131/142 and bn123/134/145 |
| Entity sound | fd/dj.a(Entity,String,FF)V | Observer size9/get23/cast28; Entity33/37/41; f2d44/dsub45; z47; observer53; empty return64 |

Complete contact inventories find six nonbase owners on each side: portal
`ak/z`, pressure `bv/bc`, soul sand `oa/iu`, cactus `or/je`, detector `ph/jn`
and web `rn/ku`. The other 63 current native kinds inherit the proved base
return. Positive pressure and detector helper calls at PC27/PC35 remain
unfinished; their missing Class queries and mutation schedules are not replaced
with constant results.

## Original observations and retained failure

The intended family has 59,654 cases per side, 29,827 for each concrete kind.
It constructs actual original Item/Falling instances with null World, uses the
real constructor-owned bounds and watcher Byte updater, and immediately rereads
all controlled scalar bits. Pool reset/anchor operations and Entity Random seed
controls are explicit. No World, Entity or engine is substituted, no final
reference is replaced, and no engine timeout/work cap is imposed. Uncontrolled
Item-constructor Math entropy is retained separately and is not normalized into
movement comparisons.

All 59,654 client records qualify, including all raw field rereads, actual pool
identities/transitions and full exception traces. The client process has a
durable receipt showing natural exit code 0. Its 38,664 completed no-clip calls cover both
kinds and fit the qualified composed arithmetic model. Its 10,816 ordinary
null-World cases stop at the first real collision call after completed smoothing,
web, watcher and pooled-box effects. Source-derived first-query classifications
are PC394 for 8,112 cases, PC313 for 312 and PC232 for 2,392. These PCs are
joined from actual source/state and trace frames, not claimed as VM-emitted
bytecode offsets. Remaining rows cover watcher/sneak, fall, concrete damage/fire,
wet and selected actual source-unused-World contacts.

The server evidence contains exactly 24,109 intact records: 19,332 Item no-clip,
4,741 Item normal prefixes, 30 Item watcher reads and six Falling no-clip rows.
Each matches the corresponding complete client payload bytes. This is not a
simple prefix: interior joined records, gaps and a trailing partial record are
excluded unchanged. There are 35,545 missing or corrupt cases. No complete
server Falling or direct-effect corpus, or paired 59,654-row family, is claimed.

Disk exhaustion interrupted server evidence writing. The original producer's
final report and server result receipt remain zero bytes; the server emitted
a capture-helper stream-write failure. Its exact exit code is unavailable. Read-only
closure found no original process live, and the family was not restarted or
terminated. The complete client receipt and the precisely enumerated intact
server subset support only their stated qualification.

The initial in-memory JAR/tool/class before-pin objects were intended for the
failed final report and did not survive. Qualification joins announced source
hashes, frozen copies, durable commands and the complete client process/stream
receipt. The frozen producer enforces the known original JAR hashes before
launch; independently checked current JAR/tool/class hashes are recorded as
after pins. They are not relabelled as missing historical before receipts.
Failed/empty receipts, corrupt rows and all raw streams remain retained.

Only the actual complete composed bodies are observed. Numerical candidate
analysis does not establish a universal primitive ABI or a compiled native
comparison. Ordinary original collision/step/contact/environment completion,
nonnull-World traversal, full server coverage and native movement replay remain
unqualified. Direct fire-helper calls do not prove later World burning or fizz.

## Native reproduction

Build `mc_test_entity_movement` and `mc_test_world_movement` with the documented
strict compiler settings. Their finite constructed contracts can be captured
with fresh work directories:

    python tests/tools/world/test_runtime.py --executable <mc_test_entity_movement> --scenario world-entity-movement --work-dir <new-movement-root>
    python tests/tools/world/test_runtime.py --executable <mc_test_world_movement> --scenario world-movement-dependencies --work-dir <new-world-root>

These commands run native contracts, not an original capture or original/native
comparison. The focused host receipts and remaining validation limits are in
the native contract. Full-suite hosted CI qualification remains separate from the focused local
results. Original JARs, raw research and private machine receipts stay
outside version control.
