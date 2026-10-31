# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. The native runtime needs no JVM after local resource import.
The client remains pre-playable; the dedicated server is unfinished.

The default client opens the main menu and genuine World selection. Saved-World
loading attaches the real Player, renderer and particle manager. A focused
loaded frame composes terrain, Sky, clouds, the empty hand and GuiIngame.

World ticking now constructs all eleven default Mob kinds, runs their spawn
predicates and publishes successful candidates. Random Chunk work completes
361 coordinates and 28,880 samples before the client reaches ambience at
`Minecraft.k:1311`. Grass, Leaves, Ice and Snow Block execute their concrete
client ticks, and weather can construct a genuine Lightning Entity. A selected
Spider jockey retains its separate unfinished mount after Skeleton publication.

All 75 selected targets build on Windows MSVC Debug and Linux Clang18 ASan/UBSan.
All 88 Windows tests and 84 Linux tests pass. Original comparisons cover
14 complete random-Chunk profiles, 18 direct Block profiles and finite Mob and
Lightning constructor/predicate cases. Independent constructor entropy and
whole-frame visual correspondence remain separate from those results.

Mob ticking, persistence and drawing, further Block overrides, ambience,
gameplay actions, inactive saving and general focus-loss handling remain
unfinished. These checks do not establish playable single-player.

- [Startup and current client paths](Startup)
- [Build instructions, test results and scope](Build-and-tests)
- [World simulation](World-simulation)
- [Storage and original-reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)
