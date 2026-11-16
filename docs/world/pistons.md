# Piston activation

Saved stable client Pistons can enter the original Lever notification and
Piston block-event bodies. The path includes power checks, the movement scan,
pushability, moving-block installation, actual Piston Tile construction,
extension/retraction, Head removal/notification and existing Piston ticks.
The World block event is synchronous; it is not a scheduled-tick queue entry.

## Gameplay order

Lever activation first changes metadata. That setter can notify the Piston
before the Lever reaches its own click sound. A successful Piston event can
therefore emit `tile.piston.out` or `tile.piston.in` before `random.click`.
The original Piston suppression flag remains set at the Piston sound and is
cleared before the later Lever click.

Extension checks the original twelve-block limit and each encountered Block's
push rules. It moves from the far end toward the base, installs real moving
Blocks and TileEntities, then creates the moving head. Sticky retraction uses
the reached actual front Tile/Block and can create both a retracting base Tile
and a returning carried Block Tile. Normal retraction leaves a pushed Stone
in place.

Pushability uses the Block's virtual movement rule. Beds, doors and pressure
plates return 1; rails and ice return 0. Those overrides return before reading
Material. The separate [mobility packet](../../tests/reference/block/piston_mobility/README.md)
records both calls on all 96 registered original Blocks. Native controls use
both Registry sides and separately authored NULL-Material receivers.

Ordinary finite ticks preserve the original progress sequence: copy current to previous,
advance by one half, cap at one, then finalize on the following tick. Bounds,
Entity pushes, invalidation, moving-block removal and restored Block hooks
use their existing real implementations. Neither rendering nor the test loop
sets progress or forces the Timer to obtain a transient frame.

## Defining code and failures

Reached allocation and Class checks use explicitly supplied defining World/
Tile namespaces. The client activation context passes that dependency through
Lever notifications, Chunk hooks, recursive setters and Piston settlement.
The evaluated World remains a separate source argument. Missing native code
context refuses only when the reached descendant needs it; it is never
inferred from a later World or reconstructed from an error string.

The detailed Tile allocation entry performs the same single allocation and
active-use attempt as the existing entry. It returns the actual cached base
Registry or Piston initializer cause without retrying. Native allocation
budgets are policy controls, not evidence about Java out-of-memory timing.

Failures retain reached metadata, installed Blocks/Tiles and suppression
stores. A sound callback failure after movement does not undo that movement.
Checked native cleanup does not finish the event or replay a notification.
Execution-owned mutation descendants that have not acquired the required
retained ownership remain explicit frontiers.

Ordinary neighbor notifications preserve the detailed failure from their
single actual Chunk acquisition. A separate native control loads a saved Item
from a second Chunk, reaches a failing Entity-added producer and retains that
producer's actual receipt. Checked retirement remains busy until the receipt
is released. The failure-only projection adds no second getter or successful
diagnostic reset; its diagnostic stack frame ends before a Block hook recurses.

## Validation scope

The native gameplay controls start from serialized stable Blocks. They cover
normal push, sticky pull, twelve pushed Blocks, the thirteen-block refusal,
Obsidian, Bedrock, missing defining context, a post-move sound callback failure
and a preconfigured Piston class allocation failure. Moving Tiles, progress,
World Random and source results are never authored after execution starts.
The activation scenes face +Z; this does not qualify every gameplay direction
or arbitrary interacting circuits.

The [original activation packet](../../tests/reference/world/piston_activation/README.md)
contains two ordinary original processes and six scenarios, with 48 phase
snapshots and 94 actual observer callbacks per mode. Raw World Random and
Piston sound pitch remain separate unsaved-entropy observations. Exact click
pitch, callback order, Block arrays, Tile transitions and list membership are
compared by the native replay. All 48 phases, 94 callbacks and 315 independently
hashed array planes match. Passive Chunk Map encounter order is retained
separately: all observed orders happen to agree, while keyed membership does
not establish general Map iteration parity. The original and native Random
states and Piston pitches remain separate; no Random draw count is inferred.

The [moving renderer](../client/piston-rendering.md) has a separate finite
original/native geometry comparison. The visible empty-front SDK scene uses
ordinary input, Timer/World ticks and save/reselection. Its Piston came from
the saved input; placing a new Piston Item and its orientation hook are not
qualified by that scene.

The pushed-Stone SDK profile currently reaches the existing DK multiple-new-
Tile Set-order frontier before the World renderer publishes its special-Tile
list. It records both genuinely created moving Tiles and closes the actual
Frame, while retaining a failed result. Sorting native addresses, assigning
construction ordinals or injecting captured Java hashes would not implement
that dependency. Full Piston gameplay, broader saves, sustained play and FPS
qualification remain incomplete.
