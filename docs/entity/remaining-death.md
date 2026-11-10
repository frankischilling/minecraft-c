# Slime, Squid and Ghast death

These client receivers use the same inherited Living death body as the other
supported LivingEntities. It runs attacker callbacks, stores the dying flag,
tests the current World's remote flag, dispatches the concrete drop method,
then reloads World for the client's status call. Source callbacks and native
cleanup retain their existing ordering.

Slime's drop-ID method reads signed Byte 16 through its actual DataWatcher.
Only a value of 1 selects Slimeball, Item ID 341. Every other value selects 0,
so the inherited drop method skips its quantity draw and Item creation. The
comparison uses the narrowed stored Byte, including a requested size of 257
which the setter stores as Byte 1.

Squid overrides the drop loop. It draws `nextInt(3) + 1`, then creates a new
ItemStack for each Ink Sac: Dye Item ID 351, count 1 and metadata 0. Every loop
allocates the RAW Stack before reading the Item static and entering the actual
Item-based Stack constructor. A null Item fails at that constructor's `bf`
read; it does not become an earlier quantity or static-get failure. Each
published Item has its own Stack identity and pickup delay 10.

Ghast's drop-ID method selects Gunpowder, Item ID 289, before the inherited
`nextInt(3)` quantity loop. Fatal damage/drop dispatch remains separate from
[Slime's later removal and splitting](slime-removal.md), which now has its own
shared body, retained cleanup and direct original comparisons. Ghast behavior
remains incomplete.

## Original evidence

The supplied client archive SHA-256 is
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
These unchanged method Code attributes were inspected directly:

| Declaration | Bytes | SHA-256 |
| --- | --- | --- |
| `uw.v()I` | 10 | `9cdaec6ffb2719048aa198187707c240a8058e21580f229c38fd30b19fdc4cba` |
| `uw.j()I` | 17 | `852383ee42e88debc747dff14536a4646e25739c028e8c5dd70b58744ecec82a` |
| `xt.q()V` | 43 | `f4eb0d4f853bc80650b680a268c25ded0b84d35a9d8358949952cd0af8765f2b` |
| `bp.j()I` | 7 | `55af7ef6d3a16c171f76382cd1100814c0226e9b151524fbe0b33ce1caf807a7` |

Slime invokes its size getter at `uw.j:1`, reads the Slimeball static at 8 and
returns it at 14, or returns 0 at 16. Squid draws its quantity at 5, allocates
the Stack at 19, reads `gm.aU` at 23, invokes `iz(Item,int,int)` at 28, and
calls the Stack-drop method at 32 before returning at 42. The Item constructor
reads `gm.bf` at `iz:2`. Ghast reads `gm.K` at `bp.j:0` and its `bf` at 3.

An Arrow passed as the original damage source also reaches the inherited
literal death callbacks. Client `sl` directly extends `sn` and declares neither
`c(Lsn;I)V` nor `a(Lls;)V`; its class SHA-256 is
`2ba2e27fbc558a78b87c26316968cd5e9595656b96c861b2975cae79201d3a45`.
Both `sn` declarations contain one return instruction, with Code SHA-256
`149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d`.
The real source occurrence remains retained through both callbacks. Wolf's
later shooter resolution does not replace that earlier death-callback argument.

The same descriptor audit covers all 39 represented client class literals and
their omitted intermediate parents. Only `gs` declares overrides; the concrete
LocalPlayer follows that Player path. Other canonical concrete sources inherit
the two literal `sn` returns, including Item, FallingSand, Minecart, Lightning
and particles. The abstract Player query literal is rejected as a concrete
callback receiver. A fatal tame-Wolf test with an actual Item source first
failed at the old whitelist; it now preserves the Item argument, halves damage
40 to 20, completes both callbacks and stores the Item target.

## Native controls

`entity.remaining_death` uses real client Worlds, cached Chunks, Registry,
actors and published Items. The controls cover seeded quantities, narrowed
Slime sizes, remote-World skips, each Squid publication failure, active cleanup
refusal, callback mutation of Random and the Item static, late Registry
admission, Slime watcher failures and each remaining concrete attacker as the
source of both literal inherited callbacks. The latter retains the real source
occurrence through both callbacks and releases it after completion. The separate
Wolf damage controls cover fatal direct-Arrow and Item sources. Checked cleanup retains
published Items and never repeats the source quantity draw or drop loop.

These are authored native component controls plus static original bytecode
evidence. They do not by themselves establish executed-original death, complete
Arrow hits, World tick integration, graphics, audio, extended play or FPS parity.

The later Slime removal packet executes 25 direct K calls across 24 profiles;
eleven Windows native records match its declared comparison fields. A separate
native public-Mob test advances 21 death ticks through splitting and the
particle tail. Those results do not extend this fatal-drop comparison or
qualify complete execution-owned environment/particle continuation.
