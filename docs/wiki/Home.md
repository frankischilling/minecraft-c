# minecraft-c

A C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows and Linux.
Development focuses on the single-player client. Full parity and unrestricted
playability remain incomplete; multiplayer and the dedicated server follow
the client work. Account authentication is the only approved gameplay exception.

The latest [combat work](Combat-and-projectiles) adds Wolf, Slime, Squid and
Ghast hurt/sound/drop paths, including Wolf pack/owner decisions and counted
UTF16 comparison. Ten finite records match unchanged original calls. Earlier
Monster death, Skeleton Arrow/Bone loops, Creeper's record and Player kill
callbacks keep their separate comparison. Constructor and Item entropy remain
independent; no original audiovisual equality or FPS improvement is established.

## Start here

- [Startup and controls](Startup)
- [Build and resource import](Build-and-tests)
- [Combat and projectiles](Combat-and-projectiles)
- [World simulation](World-simulation)
- [Storage and reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
- [Parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
- [Test results and limits](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
- [Earlier evidence](History)

Import original resources locally. After import, ordinary runtime operation needs
no JVM, original archive or account services. Original resources and reference
binaries remain outside the distributed project.
