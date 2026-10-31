# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. The native runtime needs no JVM after local resource import.
The client remains pre-playable; the dedicated server is unfinished.

The default client opens the main menu and genuine World selection. Saved-World
loading attaches the real Player, renderer and particle manager. A focused
loaded frame composes terrain, Sky, clouds, the empty hand and GuiIngame.

World ticking now constructs all eleven default Mob kinds, runs their spawn
predicates and publishes successful candidates. Random Chunk work completes
361 coordinates and 28,880 samples. The following random display call completes
all 1,000 samples and 6,000 World Random draws, then EffectRenderer ticking
returns. Repeated play reaches an actual Mob's unfinished active tick at
`Minecraft.k:1216`. Grass, Leaves, Ice and Snow Block execute their concrete
client ticks, and weather can construct a genuine Lightning Entity. A selected
Spider jockey retains its separate unfinished mount after Skeleton publication.

All 78 selected targets build on both native hosts. All 93 Windows and 89
sanitized Linux tests pass. Windows and Linux each pass 72,124 random-display checks. All 111 original
state rows and 2,162 sound/particle events match exactly. Earlier comparisons
cover random Chunk work and finite Mob/Lightning constructor and predicate
cases. Independent constructor entropy and whole-frame visual correspondence
remain separate from those results.

Mob ticking, persistence and drawing, further Block simulation overrides,
concrete display particles,
gameplay actions, inactive saving and general focus-loss handling remain
unfinished. These checks do not establish playable single-player.

- [Startup and current client paths](Startup)
- [Build instructions, test results and scope](Build-and-tests)
- [World simulation](World-simulation)
- [Storage and original-reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)
