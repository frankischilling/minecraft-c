# Bed reference-reader CPU comparison

The public World [Bed packet controls](../../tests/reference/world/bed_interaction/test_packet.py)
spent most of their time revalidating the same original packet while testing
changed native payloads. The source validator checks all 85 original profiles,
plane fingerprints, 408 selected calls and 20 live Code records. Repeating
that work for each of 1,110 native-payload mutations added no new source input.

The public `native_rows` entry still validates every supplied original packet.
It then delegates to the same parser body. Only the internal mutation loop
uses that parser over its already validated original. A final deep comparison
requires the whole original to remain unchanged. All 1,167 corruption controls
remain: 1,110 native payloads, 23 original-packet mutations, 24 provenance
mutations, five JSON cases and five sparse-plane domain/order cases. Original
mutations still execute the complete source validator. No parser condition,
comparison field, assertion or watchdog is removed.

## Fixed work and measurement

Both cohorts ran on the same Intel Core i9-10900F, ten cores and twenty logical
processors. Windows 10 build 19045 used CPython 3.14.3; Ubuntu under WSL used
CPython 3.12.3. Each host ran three serial baseline/applied pairs in fresh
normal Python processes, six processes per host. Windows ran baseline first;
WSL reversed that order. The two hosts ran serially because they share the
physical CPU. Other builds and native tests were terminal during measurement.

Each process used the same immutable public packet and complete 85-row native
sample. It completed all 1,167 rejection controls and the 2,569-value projection.
Baseline/applied stdout was byte-identical within each host; all processes
exited zero with empty stderr. Before/after checks preserved every packet,
helper, reader snapshot and sample fingerprint. The original packet and its
historical capture provenance did not change.

Wall time spans the complete fresh process. CPU time includes its complete
user and kernel time: Windows `GetProcessTimes` after child completion, or
serial `RUSAGE_CHILDREN` differences after WSL child completion. These are
separate measurements; WSL wall time includes substantial additional waiting.

| Host | Pair | Version | Wall seconds | CPU seconds |
| --- | --- | --- | ---: | ---: |
| Windows | 0 | Baseline | 41.890857 | 41.843750 |
| Windows | 0 | Applied | 13.903660 | 13.828125 |
| Windows | 1 | Baseline | 42.040039 | 41.906250 |
| Windows | 1 | Applied | 13.888430 | 13.796875 |
| Windows | 2 | Baseline | 41.867438 | 41.765625 |
| Windows | 2 | Applied | 13.924269 | 13.875000 |
| WSL | 0 | Applied | 21.589597 | 14.877467 |
| WSL | 0 | Baseline | 58.144066 | 42.223755 |
| WSL | 1 | Applied | 21.545398 | 14.849493 |
| WSL | 1 | Baseline | 57.104686 | 42.503310 |
| WSL | 2 | Applied | 20.973624 | 14.850092 |
| WSL | 2 | Baseline | 57.370220 | 42.220716 |

Windows median CPU falls from 41.843750 to 13.828125 seconds, **66.95295% lower**.
Its baseline range is 41.765625–41.906250; applied is 13.796875–13.875000.
Median wall time changes from 41.890857 to 13.903660 seconds.

WSL median CPU falls from 42.223755 to 14.850092 seconds, **64.83001% lower**.
Its baseline range is 42.220716–42.503310; applied is 14.849493–14.877467.
Median wall time changes from 57.370220 to 21.545398 seconds. Wall ranges are
57.104686–58.144066 baseline and 20.973624–21.589597 applied. Windows wall
ranges are 41.867438–42.040039 and 13.888430–13.924269 respectively.

## Source identity and limits

The comparison used authored snapshots of the two reader files. The applied
public bytes equal the measured applied snapshots. AST checks preserve the
public validation and parser bodies; reversing the small extraction and
test-only changes restores the exact baseline bytes.

| File | Baseline SHA256 | Applied SHA256 |
| --- | --- | --- |
| `run_native.py` | `462d508d9a37ca5090847ada40abe0f7311ea6bfce43118858e99c729d149050` | `bac70c2b42619463642d792c7e156625349023fa6d59b36ccfb3c3b0bf4a3717` |
| `test_packet.py` | `4092d6ded025e04470829afdf3aa144e6dd7d8353df15ca7b6ca9471cc65856c` | `46612d3664f5ee0a6d021197b60ea7de3c70f1266f5a427d45510f8745593138` |

The current controls run without a JAR, JVM or original assets:

```sh
python -B tests/reference/world/bed_interaction/test_packet.py
python -B -O tests/reference/world/bed_interaction/test_packet.py
```

Raw process records and full precision timing receipts remain private. The
figures above are selected numeric observations. This cohort measures normal
Python reader-control CPU and wall time. It establishes no optimized-Python
speedup, native execution speedup, whole-CI percentage or gameplay FPS gain.
