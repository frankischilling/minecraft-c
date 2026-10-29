# RenderGlobal construction

The canonical `n` owner constructs the reached RenderGlobal state on the actual
App, Engine and Display. Normal startup calls it at625, publishes `g628`, applies
the viewport and constructs the particle renderer before publishing `j660`.
Resource-thread allocation664 is the next unfinished startup operation.

The interface is [render_global.h](../../include/mc/client/world/render_global.h).
Cold allocation is separate from source construction. Construction retains
failed arrays, buffers, batch owners and GL definitions, with their reached
source stores and native causes. A completed constructor is not a rendered World.

## Source and device state

Construction creates the direct buffers, two large display-list ranges, optional
occlusion-query names, constructor fields and four batch owners. It defines
the star list and the upper/lower sky planes with the canonical tessellator.
List names come from the actual Display allocator, shared across independent
Engine namespaces. A null App fails only after the earlier constructor effects;
the control does not substitute another App or infer its fields from a view.

Logical star generation submits four vertices for each accepted star. The
original tessellator's `b=true` profile expands those quads to triangles in
order `[0,1,2,0,2,3]`, so a compiled list contains six records per star. Each sky
plane contains 169 quads and 1,014 submitted triangle vertices. Upper sky uses
169 draw batches; lower sky uses one. These source counts and submitted records
are separate observations.

The compiled-command inspection API borrows the actual immutable records. The
device control verifies every expanded star position and the exact copied XYZ
records for stars and both planes. It checks the vertex-only enabled-array
profile; disabled attribute slots are not asserted equal. No regenerated
geometry or captured pixel fixture supplies those answers.

## Ownership and retirement

The owner holds its scene namespace and live Display context independently of
the App field publication. Generated lists/queries and failed candidates remain
owned until native retirement. An external occurrence makes strict retirement
BUSY before revocation. Native occurrence release can consume one hold while
another owner remains.

Native teardown releases the scene guard before terminal namespace resources,
retains failed cleanup state and preserves the same slot for retry. It does not
invent a source deletion call, reverse completed stores or retire another
namespace's resources. Startup retires RenderGlobal and particle users before
Engine effects and the shared graphics authority; Display stays alive throughout.

## Evidence and remaining work

The current selected Windows cohort passes the CPU test, a real device profile
with 16,723 checks, normal startup and allocation-bound startup cases. The
device profile constructs a second independent RenderGlobal after genuine
startup has published its own `g`, checks a real sticky GL error and viewport,
and completes shared/failed-owner cleanup. The earlier failed test expected
quad counts from the submitted triangle list; its evidence remains separate.
Only the test expectations and added record checks changed.

Clang's selected sanitized cohort covers the CPU constructor and affected
owners. Its selection excludes this device profile and normal default-directory
startup. See [the complete scope](../testing.md#startup-scene-construction-and-typed-settings-save).
Star/trigonometric/GL correspondence to the original remains unverified.
World attachment, dynamic chunk rendering, sky drawing in the real frame loop
and gameplay remain unfinished.
