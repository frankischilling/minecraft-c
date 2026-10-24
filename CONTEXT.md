# Minecraft Beta 1.7.3

Names describe the game concepts present in Minecraft Beta 1.7.3.

## Language

**Redstone**:
Redstone dust and the circuits formed with it, switches and powered blocks.
_Avoid_: Wire as the name of the redstone subsystem.

**Redstone dust**:
The redstone item and its placed form, which carries power along connected blocks.

**Chunk**:
A 16-by-16 column containing 128 layers of blocks and the entities in that part of a World.

**World**:
The blocks, entities and simulation state belonging to one dimension.

**Overworld**:
The surface dimension, with dimension ID 0.
_Avoid_: Normal as the name of this dimension or its terrain generation.

**WorldProvider**:
A dimension's environment, including its sky, lighting and biome source.

**ChunkProvider**:
A World's source of loaded and generated Chunks.
_Avoid_: WorldProvider for a Chunk source.

**WorldChunkManager**:
A World's source of biomes, temperature and rainfall.

**TileEntity**:
Additional state associated with a block, such as a chest's inventory or a moving piston's state.
_Avoid_: Tile for this object.

**WorldAccess**:
A listener connecting changes in a World to rendering, sound and other systems.
_Avoid_: World observer as the name of this interface.

**LivingEntity**:
An Entity with health and living behaviour, including Players, animals and monsters.
_Avoid_: Mob for this shared base class.

**Carried block**:
The block type being moved by a piston.
_Avoid_: Carrier as the name of the block.

**MovingPistonBlock**:
The temporary block containing a moving piston's TileEntity.

**GameSettings**:
The client's options, including graphics, sound, controls and the last server address.

**KeyBinding**:
A named client action and its assigned key code.

**GameOption**:
A client option such as music volume, render distance or view bobbing.
