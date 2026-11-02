# Held Item mining speed and harvesting

The client Inventory now executes the actual ItemStack mining virtuals for
nonempty selected cells. `ix.a(Block)` preserves its distinct main-array reads
and binary32 multiplication. `ix.b(Block)` preserves the earlier Material
short circuit and the `f_(selected)` route into either main or armor slots.
The supplied Registry identifies the actual client code namespace independently
of nullable Inventory Player and Block arguments.

ItemStack speed first calls its own Item getter. ItemStack harvest directly
loads the Item array, then reads the Stack ID and invokes the reached Item.
The implementation preserves that order and each wrapper's source failure
frames. Neither operation checks positive Stack count or remaining durability.
No Inventory dirty store, Stack mutation, allocation or World action is added.

All 38 represented Item Classes resolve to the supplied client's bodies:

| Operation | Concrete behavior |
| --- | --- |
| Default speed | `gm.a(IZ,UU)` returns 1 without reading either argument. |
| Tool speed | `dq.a(IZ,UU)` compares the live effective-array elements by identity and returns its distinct speed field. |
| Sword speed | `qd.a(IZ,UU)` compares Block IDs and returns 15 for Cobweb, otherwise 1.5. |
| Shears speed | `bl.a(IZ,UU)` returns 15 for Cobweb/Leaves, 5 for Wool, otherwise the base result. |
| Default harvest | `gm.a(UU)` returns false without reading Block. |
| Spade harvest | `wc.a(UU)` compares the two Snow singleton identities. |
| Pickaxe harvest | `au.a(UU)` preserves ordered singleton tests and ToolMaterial tier checks, then compares Rock and Iron Material identities. Gold retains tier 0; Obsidian requires exactly tier 3. |
| Sword/Shears harvest | `qd.a(UU)` and `bl.a(UU)` compare the Cobweb Block ID. |

The [original packet](../../tests/reference/item/mining/README.md) contains 19,594
exact rows covering all 202 registered Items, all 96 Blocks and NULL Block,
with six results per row. Three original processes agree. Native controls also
cover phase-transparent Item reads, singleton-versus-ID comparisons, nullable
effective-array elements, ToolMaterial null failures, Inventory armor routing,
Material short circuits and wrapper error frames. A real controller/Player/
World/Chunk scenario reaches the held Wooden Pickaxe strength calculation.

The matrix qualifies these finite CPU bodies. It does not qualify cold
initializer failures, original rendered frames, extended gameplay,
dedicated-server behavior or an FPS improvement.

Held Item destruction now follows the same controller path after the Block
removal and harvest decision. `os.b` retains its captured Stack across reached
callbacks. The Item virtual reuses the shared `iz.a(int,Entity)` durability
kernel: tools apply damage 1 and Swords apply damage 2. Shears apply damage 1
only to Leaves or Cobweb and still return the base false result. Other Item
Classes inherit false without changing durability.

`iz.a(IIIILgs;)V` invokes the used-Item statistic only after a true Item result.
The shared durability kernel performs the broken-Item statistic before count
decrement and damage reset. Thus Shears can wear out without a used-Item
statistic. The controller then tests exactly `count == 0`, invokes the literal
Stack callback and rereads the actual Player/Inventory for `gs.H`. A negative
count remains selected. These operations also run when the original controller
does not permit harvesting. Source failures preserve earlier Block, durability
and statistic effects.

Two guarded original processes agree on 1,947 direct Item destruction rows,
384 actual Stack wrapper rows and six complete controller Stone removals.
The selected observations cover all 202 Item IDs, all 38 actual Item Classes,
durability boundaries, signed overflow, signed counts, used/broken statistics,
Inventory clearing and Cobblestone publication. Network/listen/native loading
are denied and no attempts occur. These are CPU scenes with actual constructed
owners and unchanged original methods.

The focused Windows MSVC Release checks pass 346,072 mining checks and all
19,594 six-result rows. The expanded combat test passes 13,279 checks and all
2,331 Item/Stack destruction rows. Six fresh controller processes pass 425
checks and match all six original controller rows. Their genuine Player,
Stack, Entity and Chunk owners complete checked native cleanup. An earlier
test cleanup correctly refused its manually published nonempty Chunk cache;
the final fixture explicitly withdraws that publication after ending Entity
memberships. No production cleanup rule changes for that fixture correction.

The destruction packet has separate hash and schema checks. Full saved-world
gameplay, concrete Block harvest overrides such as Leaves/Snow/Ice, original
pixels/audio, Linux SDK play, extended sessions and FPS remain separate scopes.
