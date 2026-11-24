# Arrow renderer source identities

`source-pins.json` records offline identities from the supplied Beta 1.7.3
client and the locally available LWJGL 2.9.0 archive. It pins the complete
551-byte `MC.a(SL,DDDFF)` body, 18-byte bridge, five-byte constructor and selected
BW, TH, texture, NW, Math and graphics dependencies. Class and Code hashes are
metadata; no original class files, instruction bytes or texture are distributed.

The original `/item/arrows.png` is a 32 by 32 RGBA PNG of 322 bytes. Its archive
identity is recorded without exporting pixels. The native contracts instead
create an authored 32 by 32 texture and retain its actual bytes separately.

This is static source evidence. It contains no executed original renderer,
original graphics trace, pixel comparison, World frame or FPS result. The native
contracts in `tests/client/render/test_arrow_draw.c` and
`tests/tools/client/test_arrow_draw.py` exercise the real native MC, NW, texture
and Display paths with declared inputs and failure controls.
