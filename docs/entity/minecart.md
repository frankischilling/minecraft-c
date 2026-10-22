# Minecart construction and detector membership

The Minecart module implements the client `yl` and server `pr` class initializer,
both constructors, their owned fields and native failure prefixes. It reuses the
[Entity base constructor](construction.md) and the existing canonical Minecart
Class. Genuine instances can enter the normal [World and Chunk publication
paths](../world/entities.md) and [Class-filtered queries](../world/class-entities.md).
The [paired source map](../reference/minecart-construction.md) records the exact
methods and separates this implementation from runtime verification.

Native construction, publication and detector contracts pass on MSVC, GCC
and Clang. The [validation record](../reference/minecart-construction.md#native-validation)
gives the sealed source and finite scenario scope. Original Minecart runtime
observations and equality remain unverified; the earlier Item observations
and rail contracts retain their own scopes.

## API and construction order

The public [constructor API](../../include/mc/entity/minecart/minecart.h) uses the
existing `mc_entity` owner and `mc_entity_status`:

| Operation | Effect |
| --- | --- |
| `mc_minecart_entity_allocate` | Initialize Entity and Minecart on first active use, then allocate one canonical RAW instance and publish one native hold. No Entity ID is consumed. |
| `mc_minecart_entity_construct_world` | Run the base constructor, allocate the own inventory array, set defaults and dimensions, then return. |
| `mc_minecart_entity_construct` | Run that same prefix, position the existing bounds, zero motion, store previous coordinates and the supplied signed type, then return. |
| `mc_minecart_entity_get_snapshot` | Passively copy own fields, constructor PCs and the actual inventory identity and references. |
| `mc_minecart_class_get_snapshot` | Passively inspect the class phase, first failure and actual initialized array graph. |

Only a matching-kind RAW instance can construct. A nullable World is accepted;
a nonnull World must borrow the same Random profile. This admission does not add
a GAME/READY check or require the World that will later receive the Entity.
The base constructor consumes an ID, creates the ordinary bounds, Random and
watcher, and takes its counted World binding at the reached store205. Minecart's
watcher hook is empty; the existing index0/Byte0 registration is unchanged.

The World-only constructor allocates a real 36-reference ItemStack array at8
and stores it in `h` at11. It stores `a=0`, `b=0`, `c=1`, `i=false`, sets the
inherited flag at36 and size to `0.98f` by `0.7f` at44. Offset is the actual
binary32 height divided by `2.0f`, stored at54. Resizing alone leaves the earlier
base-created bounds unchanged.

The positioned constructor delegates once, then positions those same bounds at
`x, y + double(actual_offset), z` at17. It zeroes the three velocities, stores
the raw supplied XYZ as the previous coordinates and stores the unrestricted
signed type at55. It does not clamp type or coordinates. Strict Java floating
association and the existing [bounds rules](construction.md#arithmetic-and-current-identity)
apply. No Item `Math.random()` draw, Stack construction, inventory call or World
insertion is added by either constructor.

## Class graph and lifetime

One EntityClasses owner represents one loaded namespace. Minecart's passive
Class literal remains selector22, directly below Entity in the canonical
ancestry; the 26 literals and 24 EntityList registrations are unchanged.
Literal lookup, ancestry and snapshots do not initialize Minecart.

First active use initializes the Entity superclass counter before the own
`j` graph. The initializer creates 31 distinct arrays: one ten-row outer array,
ten two-endpoint row arrays and twenty three-int endpoint arrays. It performs
the 60 integer stores in source order, publishes `j` at439 and returns at442.
Equal-valued endpoints still have different array identities. The
[class snapshot](../../include/mc/entity/minecart/classes.h) borrows all actual
identities and copies their lengths and values.

Class-owned `j` survives instance release. Instance-owned `h` is separate from
ItemEntity's singular Stack reference. All 36 entries remain null along these
constructors; there is no API for installing inventory contents or transferring
nonnull Stack holds. Subtype fields remain separate from the shadowed base and
Item/Falling fields. The three client-only double fields are absent on server;
inspect the snapshot's source before interpreting them.

Native allocations charge the same class ledger at each actual request. A
Minecart instance is one allocation containing its Entity header and own state;
`h` is a later separate instance request. The initializer graph is charged to
the class, not to each instance. Checked limits include the actual native sizes,
without claiming Java heap or allocation-failure timing.

Retain/release and snapshots support RAW, READY and failed instances. Final
release retires the actual inventory array and base children and balances any
construction-World binding. Class destruction remains BUSY while its Entity,
bounds or application bindings are live, then retires `j` once. Borrowed views
require exclusive live owners and expire on mutation or final release.

## Failure prefixes

Argument, kind, phase and profile admission preserve the existing instance on
rejection. Once entered, a failed constructor retains FAILED_CONSTRUCTION and
every completed ID, entropy, field, class and child-owner effect. It cannot be
retried. A later failure does not release already published `h` or the base
World/Random/watcher/bounds before final instance release.

The four [diagnostic sites](../../include/mc/entity/minecart/sites.h) distinguish
the World constructor, positioned constructor, current class initialization and
its first failed cause. Selection and entry are explicit; PC0 does not imply an
unreached or neutral site. A base failure keeps its actual child error while
the own frames identify the delegating calls. Allocation of `h` fails at the
World constructor's8 beneath the positioned constructor's2 when applicable.
Success clears the full error.

The initializer retains its first full failure. Unpublished native graph
temporaries are released; earlier class effects remain. Later active use
reports the cached failed class without retrying or resetting it. An ordinary
instance allocation failure after successful initialization leaves the class
READY. These native diagnostics do not model JVM Throwable identity or garbage
collection.

## Publication and detector path

Publication requires the World's exact bound EntityClasses owner. Matching
names, side or Random profile do not make a foreign loaded Class identical.
The wrapper retains the existing nonplayer presence, acquisition, Chunk append,
World.b append and observer order. It does not rewrite the construction World,
repair bounds or add a constructor-World equality/READY gate. Each duplicate
Chunk or World occurrence owns a separate Entity hold.

A matching [Class query](../world/class-entities.md) traverses actual normal
Chunk membership and checks the constructed bounds. Its fresh result is
independent of World.R. Each matching occurrence owns a hold; the result also
owns one World lease. A valid foreign Minecart literal is an ordinary nonmatch.
Release every result before terminal native container retirement. Entity
construction bindings and application holds have their own later releases.

[Detector contact and tick](../world/rails.md#detector-queries-and-release)
obtain the defining Block's real Minecart Class and query a pooled box. A
nonempty captured List selects activation or rescheduling through the existing
metadata, neighbor, range and scheduler calls. Contact's Entity argument is
ignored and does not publish an Entity. Real deferred scheduling requires the
radius-eight Chunk presence span; it retains separate F and E insertion effects.

The [World contract fixture](../../tests/world/runtime/rails/test_minecart_detector.c)
prepares two GAME Worlds with four real loaded Chunks each and genuine
positioned Minecarts. Its cases cover publication, duplicate/query holds,
foreign and nonintersecting queries, positive detection, guarded and duplicate
scheduling, deactivation and retained failure prefixes. It uses the existing
generic position setter to move the same bounds; that is not a Minecart
movement or rail-physics implementation. The
[constructor fixture](../../tests/entity/minecart/test_minecart.c) separately
covers class/instance construction and native budgets.

## Native validation and remaining scope

Each host passes 7,270 constructor checks across 164 cases and 196 detector
checks across 24 literal cases. Complete D native suites pass all 177 MSVC
and 179 GCC/Clang tests per host without failures, errors or skips. Controller
mirrors and lifecycle intervals pass on every host; Linux also runs ASan,
UBSan and leak checks. The separate focused Windows detector run passes its
one configured test. The [source and result boundary](../reference/minecart-construction.md#native-validation)
records the 993-file sealed tree and pending committed-source/hosted-CI binding.

Inventory operations, Minecart NBT/factory/provenance, concrete collision and
movement virtuals, active Minecart ticks, rail travel, interaction, damage,
death/drops, automatic relinking and scheduled-queue draining remain unfinished.
The existing movement, NBT and collision gates continue to refuse unsupported
Minecart paths. Construction and real detector membership do not establish
gameplay completeness or original-runtime equality.
