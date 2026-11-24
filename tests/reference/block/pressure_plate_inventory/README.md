# Pressure plate inventory leaves

Two fresh ordinary VMs observe the untouched original registered Blocks 70/72.
Ordinary `UU` initialization calls their real constructors. The observer reads
fields and invokes source methods; it never writes a source field or directly
invokes the bounds setter. No display, GL context, World or native game runs.

Both receivers inherit `UU.b(I)` (white), `UU.b()` (type 0) and the live `bm`
texture leaves. `BV.g()` overrides the inventory shape hook: its actual call to
`UU.a(FFFFFF)` changes the shared bounds from the constructor shape to
`(0,.375,0)` to `(1,.625,1)`. A second actual invocation retains those bounds. The
packet preserves six signed metadata samples and their 36 side/metadata texture
pairs per receiver, all six raw double bounds, resolved declaring owners and
complete original `UU`/`BV` method inventories with seven relevant Code pins.

`observations.json` retains both stdout streams, including ordinary class-init
banners. `finite.json` selects the two emitted rows without replacing values.
`packet.py` audits them without Java or original assets; `test_packet.py` rejects
corrupted observations, source pins and operand/shape substitutions. This is
finite scalar evidence, with no completed rendering, pixel or gameplay claim.

Capture with a verified supplied JAR and JDK 17:

```text
python tests/reference/block/pressure_plate_inventory/capture.py --output .local/pressure-plate-new --jdk <JDK17>
python tests/reference/block/pressure_plate_inventory/test_packet.py
```

The first private capture's original processes succeeded; its extraction step
rejected the two legitimate class-init banners. The final capture explicitly
checks and preserves them. No original result from that failed extraction was
substituted into the final evidence.
