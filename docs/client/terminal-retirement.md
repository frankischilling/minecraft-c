# Native client terminal retirement

The native terminal prefix ends source admission and releases selected local
call and renderer occurrences while keeping the application and World graphs
live. It is one way: a prepared owner cannot resume frames or source loading.
Successful preparation does not authorize World destruction. Explicit native
dispatcher retirement and first-scope client World admission now follow it;
normal shutdown and the complete outer destruction coordinator remain unfinished.

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

## Client admission uses actual owning slots

The transition owner's passive terminal admission supports one intact,
successfully published selected load after its real preparation prefix. It
passes the addresses of `state.player`, the load's separate `h_occurrence` and
App's owning `view_occurrence` to the lower World audit. The load's
`i_occurrence` is checked as a mirror and never counted again. Only App's
`active_world` and the effect owner's actual World slot add direct World anchors.

Controller accounting uses the transition's real base-owner slot and every
actual captured controller slot in that load. The current App controller is
only an alias. Effects accounting similarly requires the App-retained owner
and the load's actual capture, with matching namespace, Engine and World and
four empty particle Lists. Neither inspector creates or clears anything.

Input has a separate gate: its actual holds must match the Load allocation and
the one Player's owning `dc.a` field. Player aliases do not multiply that Input
occurrence. An outside Input hold can borrow Settings without adding a World
binding, so the lower World count alone cannot detect it.

All identity and external-hold checks precede any destructive Load cleanup.
Failure preserves inventory output, source outcomes, cleanup receipts and the
live owners. Admission caches no permission. Failed or partial loads, changed
controllers and partially consumed tails remain outside this first scope.
The selected source Handler audit then counts the actual Load, World sidecar,
SavedData `source_a` and loader `store` slots. It separately checks the loader's
single owning occurrence and its retained input prefix. Store-wide admission
rejects unknown cache registrations, reservations and unfinished work on every
entry, including generic stored data. These checks run before effects, Player,
App slots or World containers are consumed. The [World contract](../world/terminal-retirement.md)
records the complete scope and the borrowed compression-buffer proof.

This source join requires a completed client LOAD_OR_CREATE World. Raw, failed,
direct-metadata and copy entries remain outside that first scope. Complete
Format, App and shared-service destruction authority remains separate.

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

The lower [World admission](../world/terminal-retirement.md) counts exact
Entity, TileEntity, Provider and static Piston owners without releasing them.
The selected-load adapter supplies the client anchors described above.

`mc_client_bootstrap_terminal_dispatchers` requires the actual completed
transition prefix and keeps App, Display, Engine, Font, Settings and the full
World graph live. It passively captures an existing LL association using the
exact Models, Registry and WorldClasses identities. Absence returns null;
cleanup neither binds nor initializes LL. Cold and failed associations remain
discoverable, and the lookup retains the same inactive retiring owner for retry.

The startup helper ends only its actual TH consumer record and the borrowed
aliases backed by that record. It latches a one-way native policy. Completed
repeats avoid consumed global handles; an earlier destructive startup retirement
cannot be upgraded. No App-owned LL consumer exists in the current runtime, so
none is manufactured or detached.

The outer primitive then retires existing LL, followed by actual TH hands and
TH classes, through their checked owners. Every foreign or active refusal keeps
the remaining graph for retry. A retained Piston CV or dispatcher remains a
real World root. Global retirement can consume earlier admitted slots before a
later refusal; callers retain the same native handles and dependency graph.

Preparation alone performs none of that dispatcher retirement. Even after the
selected-load Handler and World audits succeed, Format and other service roots
still need outer admission. The ordinary menu path needs a retained old-World queue so
source reference changes do not destroy a graph still held by those roots.
The full-retire APIs remain explicitly destructive; they are not an outer
external-root admission check.

The normal menu-run shutdown path has not adopted these explicit primitives.
RenderGlobal's delay and LL/TH frame invokes at N44/N80 also remain separate
integration work. Full World frames, ordinary menu World replacement and
gameplay lifetime parity remain unfinished.

## Qualified native scope

All 39 selected targets build on Windows MSVC Debug and Linux Clang18 with
ASan/UBSan. The selected CTests pass 48 Windows cases in 69.44 seconds and 46
Linux cases in 98.83 seconds, with source inputs unchanged and no sanitizer
findings. All 1,607 C/header/fragment inputs pass clang-format18.1.3.

The RenderGlobal CPU test passes 28 checks on both hosts. The Windows device
test passes 28,892 checks, including the sticky terminal policy, real callback
receipt, foreign membership and final-retire controls described above. Four
nonloading menu-loop modes pass preparation, repeat and source-frame refusal
checks while retaining their actual App, screen and service owners.

The Windows menu test now includes a separate real loaded-World terminal
profile. It completes the actual menu/Load/N prefix while preserving World,
Player and services, then refuses real extra Input, controller, effects and
Player holds. A genuine separate native TH consumer blocks dispatcher
retirement after the startup's own record ends. Releasing that caller-owned
record permits checked TH hand/class retirement while the same World stays
READY. LL is absent in this current runtime profile; it is not created during
cleanup.

The public no-anchor World audit still refuses that client graph. Admission
through the actual transition/Load owners succeeds with its three Player
anchors and two direct World anchors. The test then completes checked full
cleanup. Its wrapper preserves all pre-existing save and asset bytes and
checks all 72 original focus histories for each of the ordinary and terminal
loaded profiles. Existing native UI modes retain their prior path.

The same loaded profile now refuses direct Handler and selected-loader retains,
an independent source SavedData manager and a second real loader. Both hosts'
source-World fixture separately proves the actual four Handler slots, the
loader's independent hold count, positive retries and preserved diagnostics.
Store topology controls include a valid extra empty registration, a reservation
and a generic stored-entry lease. Each actual blocker ends through its own API.

Both hosts separately exercise passive controller/effect inspection, genuine
RAW and failed-construction owners, nonempty particle refusal, and passive LL
absence, cold, READY, quota-failed and consumed associations. Foreign Registry
and WorldClasses lookups preserve output and owners. The new lookup's active
reentry and inactive RETIRING paths remain unexecuted; no private guard writes
create those cases. Controller/effect inspector reentry also remains unexecuted.
Bootstrap retirement of a present or partially retiring LL,
two-App lifetime ownership and complete Format/App/shared-service admission remain
unqualified. Independent compression holds and source-owner active reentry are
not newly executed. Later OS close failures still require checked retries.
