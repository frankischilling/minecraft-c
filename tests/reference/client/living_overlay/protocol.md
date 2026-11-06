# LivingEntity overlay protocol

The primary capture constructs the unchanged original World, Spider, Slime,
Squid, Creeper and Cow camera, then publishes the actors normally. The App is
an explicitly prepared RAW identity; no constructor/startup completion is
claimed. Actual TH singleton and model constructors run normally. Model and
World preparation are the explicit caller inputs in `preparation.tsv`.
`profiles.tsv` supplies seven ordered calls, all at partial tick .375:

1. Spider hurt: aa5/ad0.
2. Slime size1 hurt: aa5/ad0.
3. Squid death drawing: aa0/ad5.
4. Creeper packed color: aa0/ad0, current fuse a6, prior fuse b5.
5. Creeper hurt and packed color: aa5/ad0, a6/b5.
6. Creeper no overlay: aa0/ad0, a0/b0.
7. Spider negative timers: aa-1/ad-1.

These fields are authored caller inputs before actual manager drawing. The
observer writes no fields, invokes no target methods and changes no original
Code. Constructor clocks, private/World/shared Random seeds and namespace
Entity IDs remain raw. The ordinary and read-only JDI VMs are independent.
No observed original seed or field result becomes a Native execution input.

The actual GV body uses GL_EQUAL at443; hurt color466, primary488, virtual b506,
selected color518/model540; packed color626, primary648, virtual b666,
selected color680/model702. Depth714, blend720, alpha726 and texture732 restore
in that order. GV.b delegates virtual a4; Creeper's b bridge calls its own
constant-false typed body. The source JDI records these actual calls and Code
hashes, rather than inferring them from helper output or Native results.

Observed EQUAL list replays are22,5,9,6,12,0,0. Each real model-part list holds
six source NW quads; the Native core observer submits six triangle batches per
replayed list. Geometry/model state, actual calls, predicates and material state
have distinct evidence. Full source and Native live GPU order or pixels are not
implied by matching a batch count.

The optional Windows ordinary supplement runs the same frozen compiled helpers.
Its CPU modelgraph/parts/pool, selected aa/ad/fuse and per-process complete actor
invariance are audited against the primary. Independent constructor fields stay
raw; cross-process complete actor equality is not claimed. Only current-normal
words may supplement a Native run on the actual same GPU and NVIDIA581.42 driver.
The frozen helper records GL vendor/renderer/version, not a separately queried
profile mask. No supplementary JDI or original/native pixel credit is claimed.

All actual input/tool/SDK/helper and consumed output fingerprints are immutable.
The public authored files were copied after capture with exact bytes; they were
not capture-start paths. New Python capture and comparison tools are separate
post-capture work. The optional adapter rejects missing/changed source artifacts,
helper/Code identity, class origins, Native build input/output binding and corrupt
copied observations. Missing references never become a successful comparison.
