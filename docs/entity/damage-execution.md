# Damage in the caller's execution

The private entry in `src/entity/movement/damage_execution_internal.h` connects
the existing Entity damage dispatcher to its caller's execution. Living,
Monster, Wolf and Player wrappers share their existing production bodies.
Legacy entries pass no descriptor; the owned entry requires one and preserves
the original reached virtual dispatch and early returns.

The caller supplies the actual defining runtime classes, Registry, Chunk-call
provenance and one shared Math context. A mutable victim World never selects a
different defining namespace. The frame, error and outputs remain stable and
exclusive through checked cleanup. Retained actor work can borrow that frame,
so it cannot live in a diagnostic union that another child overwrites.

## Independent outcomes

The continuation Boolean, raw Entity status and gameplay hit Boolean are
independent. A completed rejected hit permits the Arrow's original reflection
path. A native or Math refusal can leave raw Entity status OK while stopping
before a wrapper return, another child, Item publication or Inventory clear.
Cleanup never converts that original refusal into Source completion.

Living knockback, sound and Mob death enter their actual owned children. Wolf
keeps its original sitting, damage adjustment, inherited parent, retaliation,
class query, pack and owner decisions. Its query captures World at 101 before
the Class operand and pooled/expanded bounds, then invokes at 152. Player keeps
the actual ally alert, armor/statistics and retained fatal-death paths.

The module records newly acquired actor work at each actual child call. An
unrelated death created by a sound callback remains owned by that callback's
caller; the surrounding hurt operation cannot retire it. Inspect returns only
the canonical result and work acquired by this operation.

## Player drops and death

`src/entity/player/drop_execution_internal.h` connects selected and Stack drops
to the same explicit execution. Stack drop captures constructor World at 10
before later arguments and the Item constructor at 36. Publication captures a
fresh World in `gs.a(Item)` at 1 before Item at 4 and insertion at 5. Scatter and
ordinary velocity calls use the caller's actual Math namespace; Java Math
constructor/perturbation entropy retains its independent profile.

The retained Player death entry uses the same body as the legacy entry. The
optional Notch Apple at 61 precedes Inventory scatter at 68. Scatter visits all
36 main and four armor slots in Source order and reloads the live array before
each clear. False continuation, including raw OK, stops before clear40/87 and
later slots. Already returned drops, cleared slots and actual Random progress
survive failure and cleanup.

Fatal Player death does not close a Container. `gs.K` belongs to later removal;
this join adds no Container close to `gs.b(Entity)`.

## Actual failure evidence

`mc_entity_damage_execution_source_failure` reads the actually selected failed
child before cleanup. Protected callback classification requires the recorded
scope/lease tuple and actual callback cause before reading a receipt. Direct
typed null/Registry/statistics faults retain their existing authority. A typed
WorldAccess null-List fault is captured at the actual receiver read, independently
of a callback receipt. Blanket legacy `native_policy`, numeric status, operation
text and an exception-name suffix do not establish a Source Throwable.

Unknown callback classes and unqualified query failures remain explicitly
unresolved. Names and typed views borrow their original owners until checked
cleanup. The parent must preserve first diagnostic text before consuming those
owners and expire typed borrows as cleanup advances.

Math failure is a separate Value record made at an actual non-OK `require`
return. The adapter invokes the same require once. It records the requested C
caller, actual return status and immediate returned cause, with the actual Math
owner as a nonowning identity. A copied initializer PC describes that cause's
origin; it does not prove a new initializer ran after an admission refusal.
Canonical result forwarding checks complete proof/owner conflicts before any
destination mutation and preserves status separately from cause fields.

The surrounding damage operation does not sample an ambient failed Math flag
after a child returns. An independent failed Math probe inside a sound callback
therefore cannot become the later death method's Math failure. No receipt,
class owner or cleanup permission is acquired by copying this Value.

## Arrow impact

The Arrow parent captures its current nullable Living shooter at bytecode 737,
then passes that value and literal amount 4 at 741. It does not substitute the
Arrow as the source. The actual unchanged client method uses `random.drr` at
752/771, then marks the Arrow dead at 775 after an accepted hit. The existing
position, rotation, drag and gravity tail remains in the original tick body.

Only an actually reached hurt allocates the separate stable damage workspace.
Its bytes and any protected exception-name copy count against the same parent
allocation ceiling. A failed workspace remains live through diagnostic capture,
canonical child draining and checked retirement. Successful cleanup precedes
the later impact sound's reuse of other diagnostic storage.

Native unwind services the actual retained child result, its pending acquisition
or published close capsules, then the child's recorded scopes and actor work.
Forwarded summaries confer no close authority. Unrecorded descendants refuse;
the helper neither discovers arbitrary scopes nor replays Source work.

The reached Player wake at `gs.a(Entity,int):35` shares the existing
GS body. Its retained execution covers the occupied Bed update, recursive
orphan head or foot removal, foot/Ladder/Torch Item publication, and the live Player
List read after waking. Arrow closes the actual failed wake work through its
existing unwind. Other reached neighbor hooks, arbitrary cold import or generation, and
foreign graph ownership retain their separate limits. See the
[Player wake contract](player-damage-wake.md#wake-during-an-arrow-hit).

Unrestricted combat, original audiovisual equality, extended play and
measured FPS remain unqualified.
