# Audit retained source region output

The original-only capture driver reports its fresh retained directory. Audit its actual JDI owner and RAF trace without launching another original process:

```text
python -B tests/reference/world/source_region_output/audit_owner_trace.py --capture-dir <retained-directory> --output .local/source-region-owner-audit.json
```

The audit checks that each reached QJ commit receives the actual Hz QJ field and exact existing buffer/count, each RAF.writeInt performs four ordered primitive writes on the same RAF, RAM offset/timestamp stores precede actual lower seek, and cache flush closes both old descriptors before the retained old Hz reaches its closed-RAF commit. The output file must be new. This audit adds no native primitive trace or source Chunk/World caller credit.

`provenance.json` records Windows Temurin17.0.17+10 execution;
`linux-provenance.json` records the separate Ubuntu17.0.20.1 execution.
Class/Code metadata is shared where the selected metadata agrees. Distinct
capture hashes remain recorded. Java helper bytes match the qualified
captures; the parent README describes the current reproduction commands.
