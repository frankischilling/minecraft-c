# Activation close continuations

An owned Chunk activation can finish its source body while its checked World
scope close refuses. Its caller must retain that exact created scope, including
when the activation made no nested get. A pending loader attempt alone cannot
identify or authorize that close.

The [activation helpers](../../src/world/runtime/activation.c) now publish a
VALUE continuation through the required
[runtime result](../../src/world/runtime/runtime_execution_internal.h).
Each record contains the actual token/generation, activation kind, operation/site,
pre-close body status, failed operand and LOCALS_DRAINED phase. The sparse ledger
uses the existing eight World slots and acquires no additional World, receipt or
loader owner. Existing public error layouts and Entity fields are unchanged.

## Record the actual close outcome

Both participating Tile and Entity activation helpers leave their ordinary
entered World calls before closing their scopes. Early failure paths never
enter those calls. The shared native helper performs the same one checked pop
and records its native status independently of the existing source cause.

A non-OK pop can already have consumed a scope. The helper therefore publishes
only if the exact created token still names an OPEN slot after that actual pop.
The narrow check reads execution identity and inline token/slot/generation/phase
values, without dereferencing World or lease pointers. This remains usable while
a temporary native binding counter makes a complete ledger query refuse.
RETAINED or removed scopes produce no initial retry record. The actual native
error remains recorded even when no continuation is needed.

Source diagnostics stay primary. The separate result preserves the first native
ownership error before later cleanup or parent completion can report another.
The existing NULL-result routes retain their previous pop behavior. All source
bodies outside the two activation close helpers remain unchanged.

## Forward and retry explicit values

Result forwarding validates both pending-attempt and close ledgers, shared owner,
active selectors and conflicts before changing any destination. Equal copies are
idempotent; an empty no-get child does not erase a live parent continuation.
Invalid or conflicting inputs leave the destination unchanged. Callers must keep
both results on such a contract refusal rather than discard a child frame.

Native retry takes one explicit published continuation, either its actual result
slot or an equal copy. It verifies the stored value, exact checked current token
and drained phase, then refuses while that World has an unfinished actual get.
Only one checked pop follows. Refusal preserves the continuation; actual OK clears
its exact slot. Source status and the first native error remain intact.

Retry does not finish a getter, select an ambient scope, pop an unrecorded child,
release typed diagnostics, retire owners or resume source work. The actual caller
must first restore the correct scope and finish its matching returned attempt in
the required order. Arbitrary descendants without their own published close
continuation remain unfinished.

## Executed Entity descendant controls

The [native fixture](../../tests/world/runtime/world_access/test_activation_close_execution.c)
uses the canonical complete SourceSaving Item/Chest input and real constructors,
Store, Registry, loader, Entity membership and WorldAccess callback. Its callback
self-removes, observes BUSY receiver retirement, creates a real managed receipt
and returns its declared source failure. It lowers only the existing native
binding counter; the exact saved scalar is restored immediately after one actual
get, before any assertion, inspection or native recovery. No source pointer or
field is repaired, and no second getter supplies an outcome.

The callback stores no scope token. The caller selects Entity B only from its
actual returned close record; A remains the actual pending parent get. The native
sequence captures the protected receipt, rejects an altered-generation copy,
rejects premature A completion while B is current, retries B, rejects the consumed
authentic copy, finishes A and inspects its genuine failed/NONE selection before
closing its own A token. List/membership/Item/Chest/cache/pending/loader identities,
callback counts and output prefixes stay fixed during recovery. Source IO and
first native capture refusal remain separate.

| Mode | Native checks per host | Retry operand |
| --- | ---: | --- |
| error | 58 | The actual returned result slot, with ordinary acquisition diagnostics |
| null-error | 56 | An equal copied continuation, with NULL acquisition diagnostics |

An initial broader selection refused four older kernel controls per host before
native launch: their separate bundle still pinned the preceding activation code.
The retained setup failures receive no native execution credit. An explicit
projection updates only the five reviewed implementation/header pins, declares
the new dependency and approved selection, and changes the runner's bundle hash
and selection literals. Native fixtures, recipes, runtime assertions, protocol
and capture/input guards remain unchanged. Fresh execution after that projection
is recorded separately.

These two fresh processes pass **114 checks per host**. All common query and
close modes pass **913 native checks** in sixteen fresh processes, plus normal
and optimized Python controls. The 18-test selection passes Windows in 136.19
seconds and Clang Debug ASan/UBSan Linux in 58.70 seconds. Additional disjoint
regressions pass Windows 94/94 in 150.24 seconds and Linux
92/92 in 109.00 seconds. The combined selections preserve every preceding name
and add two close modes, for 112 unique Windows Passed names and 110 on Linux.
Neither selection contains skipped or unrun tests.

Typed callback/loader borrows are cleared before checked neutral retirement.
Only after the retained owners release can the real receipt and receiver retire.
Existing SourceSaving teardown flushes region handles before strict readback of
all three authored files and final code/resources retirement. Only the genuine
eight-byte session.lock addition is allowed by the complete input-root guard.
Exact stdout/stderr/typed capture and source/product/helper/six-table guards pass
for every query child. Child limits remain 240 seconds and one MiB per stream.

Normal and optimized Python each reject 944 protocol corruptions, 320 retained-red
payload changes, seven literal changes, four bundles, three real root
loss/replacements and 70 filename corruptions while preserving the first failure.
They also pass the prior 97 collision recipe checks, two close fourth-file
refusals and 31 pure canonical close byte/NBT checks. These are Python controls,
not native bootstrap or original execution.

Both builds, exact clang-format 18.1.3 over 2,286 files and three actual GCC syntax
recipes pass. The two implementation C files and fixture also passed strict
standalone MSVC, Clang 18 and GCC syntax checks using isolated copies of the actual
include graph and the exact prospective private headers. Timings above are test
durations, not gameplay performance measurements.

```text
ctest --test-dir <build> -C Release --output-on-failure -R "^world\.activation_close_execution_(error|null-error)$"
```

For Linux Debug, omit the Windows configuration argument. The common controller
tests are `tools.world_query_controls_(normal|optimized)`.

## Remaining composition

The genuine runtime control covers Entity B beneath its real parent query.
Canonical Tile activation is callback-free Map.values/addAll, so its refused-close
arm has no qualification from this callback. Consumed-nonOK, direct close retry
under a different current scope and matching-pending-child refusal also remain
unqualified where no genuine opportunity was reached. A non-OK retry is never
reported as an OK close.

Unrecorded internal/publication descendants, foreign World/joint Store and general
import/generation/provider ownership retain their external lifetime contract.
Arrow parent result storage, typed source classification and checked unwind
integration remain unfinished. Full flight, pickup, World-tick dispatch,
unrestricted single-player, original audiovisual equality and sustained FPS
remain open. These native controls include no new original JVM or visible game
observation.
