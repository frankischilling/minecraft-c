# Redstone Torch lifecycle replay

The native test runs four fresh processes: lifecycle (70), powered (32),
burnout (78), and aging (23) calls. All four processes, product copies, pipe receipts, saved
Worlds, and cleanup outcomes are retained before comparison. The reference
packet independently admits two original runs of each cohort.

Resources come from the existing Player-statistics fixture's `resources_at`:
three authored language/Achievement files read through the real file streams,
parsers, and initializers. The six synthetic saved-World templates carry
explicit Time values and seed 23. No installed assets, JAR, graphics session,
or default Random seed is supplied to native execution. Source and native
Random streams keep their own actual constructor entropy and continuation.

The comparison covers all 203 input/return/failure prefixes; all five arrays
on each canonical Chunk, including identities, hashes, and sparse changes; shared DB List values,
capacity, modCount, nullable entries, and alias history; ordered light values;
scheduled records, membership, dedupe and deadlines; World flags; SM Set
membership; callback order; and independently audited raw entropy. The aging
process constructs six Worlds in one class lifetime. No due-time dispatch,
queue drain, time/phase store, RNG reset, or work-budget change is added.

Lifecycle accounts for 17 default-constructor advances: 12 published Items,
four inferred unpublished constructions at the frozen far-X witnesses, and
one first Math holder. The other cohorts account for 0. Actual callback
counters and cross-row/constructor joins are checked. The unpublished Entity
identity, fields, Entity Random, and constructor host-clock operand remain
unobserved. The result therefore uses
`PASS_BOUNDED_OBSERVED_STREAMS_WITH_UNOBSERVED_ENTITY_FRONTIER` with
`full_entropy_qualification=false`.

Raw native collection metadata is retained. C/E/F physical JDK topology and F
iteration order are outside the comparison; native LIMIT callback refusals
remain distinct from Java ISE. Historical Source/native FAIL receipt hashes
stay separate from fresh bounded results. No loaded rendering, complete
powered circuit, or FPS result is claimed.

`test_redstone_torch.py` creates a unique evidence directory under `--work-dir`.
It checks selected source/fixture/product/resource continuity and retains
stdout/stderr and native products even on failure. Ten corruption controls
modify copies of actual successful native observations after all processes
have ended. Pure decoder/accounting controls run through
`test_redstone_torch_controller.py`. SHA256 utility vectors execute before
the native caller's Source work.
