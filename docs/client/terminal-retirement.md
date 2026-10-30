# Terminal retirement prefix

The native terminal prefix ends source admission and releases selected local
call and renderer occurrences while keeping the application and World graphs
live. It is one way: a prepared owner cannot resume frames or source loading.
Successful preparation does not authorize World destruction. The outer
dispatcher and World-root coordinator remain unfinished.

The prefix is available through
[RenderGlobal](../../include/mc/client/world/render_global.h),
[World loading](../../src/client/world/load_world.h),
[screen transitions](../../src/client/gui/source_screen_transitions.h) and the
[menu loop](../../src/client/runtime/menu_loop.h). Their existing full-retire
APIs can proceed further into destructive cleanup. Callers must distinguish
those operations from preparation.

## RenderGlobal preparation preserves shared dispatchers

`mc_client_render_global_terminal_detach` rejects an entered operation,
retained World/sky/culling/update/submit call, extra N occurrence or active
callback before ending local ownership. Once admitted, it marks that N
`RETIRING` and latches its preserve-TH policy. New source World attachment,
marking, culling and retain operations refuse that owner.

Preparation ends only N's actual WorldAccess registrations, DK/CV World
occurrences, local World occurrences and Manager hold. It leaves every TH
source field and TH's independent World occurrence intact. Equality between
TH.g and a World pointer previously stored by N is insufficient authority to
clear TH.g. A later genuine TH source bind can store that same pointer.

A cleanup failure keeps the same slot and remaining rights. The preserve-TH
policy survives calls to the legacy World-detach and owner-retire APIs. An
earlier legacy detach cannot acquire this policy afterwards; that attempted
upgrade returns BUSY. Repeating admitted preparation retries its remaining
slots. Consumed child diagnostic views expire, so callers inspect fresh state
after each attempt.

The device control uses two genuine N owners of one actual App and a direct
GAME World. It republishes the same World through TH's source setter and
attaches an actual independent Manager consumer record. An extra N hold
first refuses preparation without selecting the policy. A retained failure
receipt from the actual N WorldAccess receiver then forces BUSY after the
terminal policy is selected. The legacy owner-retire retry consumes N's local
rights while a real foreign duplicate WorldAccess membership blocks final
bridge retirement. Public removal of that membership permits final N free.

Across those attempts, all TH source fields and origins remain unchanged,
and only N's Manager hold ends. The other N remains attached to the same
World. The WorldAccess List loses N's owned registration and the explicitly
removed foreign occurrence; the other N's membership remains. The World
stays READY with its source fields, provider binding, cache/pending
descriptors, Store metadata/count, region usage and session
token unchanged. These are native quiescence controls for one App and two N
owners. They do not qualify a two-App lifetime graph, an original teardown
comparison or a complete outer coordinator.

## Load, screen and menu preparation retains full graphs

`mc_client_world_load_classes_terminal_end` ends new load admission without
releasing the class or its existing calls. Load preparation then drains only
the retained preparation, Loading, callback-failure receipt and N World-call
occurrences that the load actually owns. It selects N's preserve-TH terminal
detach and ends owned picking occurrences. A foreign retained Loading call
continues to belong to its original caller.

The prefix stops before particle-manager/effects World cleanup, controller or
Player cleanup, App `h/i/f` release, World Entity/TileEntity containers,
cache/provider retirement and SaveHandler destruction. The World, Player,
input, handler and their dependencies remain live. Source outcomes are not
rewritten as successful loads or rolled back.

Screen-transition preparation ends its load-class admission and prepares
each retained load. It keeps the screen nodes, controller and App borrows.
Menu-loop preparation first drains its owned frame/input/draw calls, then
prepares the transition graph. It retains App, Display, Engine, Font,
Settings and the remaining class services. It does not invoke source shutdown
or change source running/paused flags to simulate a completed shutdown.

Each layer refuses active entry before mutation and records its selected
terminal policy. Success records `terminal_prepared`; repeated success does
no further work. Failure preserves remaining occurrences for retry. An
already-entered legacy full retirement cannot be upgraded to preparation.
The caller keeps the complete dependency graph alive through all retries.

## Source World clear has a different body

The original Minecraft World-change method clears the view at PC16 and stores World
at PC69. Its null-World branch goes to the Player-null store355, advisory
GC358, `P=0` and return366. It skips the nonnull-World renderer attachment at
PC220. Consequently it does not run that N call's TH World setter at PC40 or
invoke LL World callbacks or LL/TH frame binding. Source clear also has
its reached statistics, Loading, sound and previous-World-save work.

The selected native load closure currently qualifies its initial null clear
without an earlier World/Player. A normal menu return from an existing World
still needs its complete source path and ownership of the old World graph.
Native preparation is separate from that source operation. It does not
replay `n.a(NULL)`, clear LL/TH globals, save a World or acknowledge source
disposal. An advisory GC request cannot replace native ownership accounting.

## Destruction and runtime adoption remain separate

Before destructive retirement, the outer owner still needs exact admission
of external Entity, TileEntity and Provider roots, including the static
Piston roots. It must end its actual consumer records and retire any existing
LL/TH associations with their Display and resource dependencies
live. LL cleanup must look up the existing association passively, then retire it
without initializing a new singleton merely to tear it down. A retained
Piston CV or dispatcher occurrence remains a real World root.

Preparation alone performs none of that global admission or dispatcher
retirement. The ordinary menu path also needs a retained old-World queue so
source reference changes do not destroy a graph still held by those roots.
The full-retire APIs remain explicitly destructive; they are not an outer
external-root admission check.

The normal menu-run shutdown path has not adopted the new coordinator.
RenderGlobal's delay and LL/TH frame invokes at N44/N80 also remain separate
integration work. Full World frames, ordinary menu World replacement and
gameplay lifetime parity remain unfinished.

## Qualified native scope

All 32 selected targets build on Windows MSVC Debug and Linux Clang18 with
ASan/UBSan. The selected CTests pass 41 Windows cases in 55.82 seconds and 39
Linux cases in 79.85 seconds, with source inputs unchanged.

The RenderGlobal CPU test passes 28 checks on both hosts. The Windows device
test passes 28,892 checks, including the sticky terminal policy, real callback
receipt, foreign membership and final-retire controls described above. Four
nonloading menu-loop modes pass preparation, repeat and source-frame refusal
checks while retaining their actual App, screen and service owners.

The existing READY loaded-World modes pass in 13.15 seconds through their
unchanged path. Those runs do not qualify the new Load/Screen/menu prefix on
a fully loaded World. The real GAME World device control and the nonloading
menu controls retain their separate scopes.
