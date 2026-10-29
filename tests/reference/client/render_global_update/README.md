# Original RenderGlobal update selector

`finite.json` records 42 calls to unchanged client `n.a(Lls;Z)Z`. The native
runner compares all 42 complete rows. Another 14 direct-array sorts and eight
direct `md.a(DK,DK)` comparisons, with comparison and exception traces, are
original research data; this runner does not count those as native replays.

The observer uses genuine Object-only N, DC and DK allocations, a real dirty
ArrayList and distinct actual DK arrays/lists. `inputs.json` declares every
prepared coordinate, visibility, dirty flag, ordinal, queue alias and null.
Pass flags, E/A, a one-null-element B list, the DK counter and shared Chunk
light marker are explicitly prepared. No N/DC/DK constructor is qualified.

Every game method remains original. Read-only JDI method events observe
compile entry identities, caller PCs442/519 and completed returns; authored
observer markers delimit cases. Loaded game method Code hashes must match
the pinned JAR. No class transformation, subclass replacement, forced return,
injected compiler failure, GL or terrain substitute is used.

Clean DKs genuinely return at7. Dirty cases execute the actual compiler prefix
and fail on the null World in real EW construction: `EW112 > DK159 > N442/519`.
They expose the counter increment, pass flags, B clear, shared light reset and
surviving queue mutations. These observations establish selector behavior and
that compiler prefix, not successful dirty compilation or visible terrain.

The cases include near/far, visibility priority, nullable cameras and Lists,
null entries, float-narrowing boundaries, duplicate identities and equal
ordinals at sizes2/3/8/31/32/33/64. The two-cell far array uses only slot1 in
these source executions. Duplicate far entries can leave an empty queue while
the source returns false: the return compares initial count with near/far
work counts, not remaining List size.

The captured runtime is Temurin17.0.17 with `-Xint`, default object TimSort and
no legacy-sort property. MD returns -1 for equal ordinals, including the same
identity. Its 85-element cohort produces a genuine sort-contract exception at
N411 after all 85 queue slots become null, with zero compiler calls. Direct
sort data retains its partial permutation and all 189 comparisons. Captures
also retain loaded JDK sort Code hashes and the JDK runtime-image fingerprint;
these tie/exception observations do not claim every Java-version sort order.

Rows separate ordinary false from failed calls. They record queue identities,
all actor flags/lists, DK counter, shared light marker, selector PC, compiler
entry order and completed returns. Native-only controls additionally check
pending-call exclusion, owner/camera holds and retirement without rollback.

```powershell
python -B tests/reference/client/render_global_update/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/render-global-update-original-fresh
```

The capture requires matching JDK17 tools and the usual four pinned SDK
metadata JARs. Game calls run under a deny guard with zero network and native
library-link attempts. The debugger's loopback JDWP transport is outside that
game-call guard. Inputs, helper classes, supplied archives and runtime images
are hashed before and after bounded execution.

`run_native.py --executable <probe> --work-dir <private-output>` needs no JAR,
JDK or Display. It checks the prepared input cohort binding, successful native
completion, unchanged inputs and exact equality of all 42 selector rows.
