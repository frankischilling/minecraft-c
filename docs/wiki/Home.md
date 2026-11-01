# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. Single-player is the current priority. Multiplayer and a dedicated
headless server remain part of the target. Usernames are local; account
authentication is excluded.

The client is still pre-playable. Existing-save loading, movement, targeting,
empty-hand Dirt mining, pickup, held-block drawing and collected-Dirt placement
have finite native scenarios. These scenarios include ordinary save/title
transitions, reselection and independent decoding of saved state.

Tools, combat, Inventory UI, remaining Item/Block actions, unrestricted
reselection, default-Far performance and extended play remain unfinished or
unqualified. The loader currently retains at most eight World loads per
application session. Original assets must be imported locally; the runtime
then needs no JVM.

- [Client startup and controls](Startup)
- [Build, resources and tests](Build-and-tests)
- [Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
- [Parity matrix and verification limits](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
- [World simulation](World-simulation)
- [Storage and reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)

Detailed implementation notes and test evidence live with the source. A passing
component or finite graphical scenario does not establish complete gameplay,
whole-frame equality or original-world round-trip compatibility.
