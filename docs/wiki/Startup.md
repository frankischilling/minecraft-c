# Client startup

The native executable opens its title screen and a separate fixed-camera World
inspection. A playable session remains unfinished.

The [normal startup owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/normal-startup.md)
connects Display, the default game directory, save manager, Settings, selected
Pack, RenderEngine, Font, color maps, EntityRenderer and RenderManager. It then
constructs statistics, input, the capability checker, SoundManager bindings,
nine texture effects, RenderGlobal and particles. Startup now constructs and
starts the resource download thread, then runs the final GL diagnostic.
GuiIngame caller allocation at offset 697 is the next unconnected operation.

The [resource worker](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/resource-download.md)
uses native HTTP/XML and retains App dependencies until stop/join completes.
The [source screen owners](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/source-screens.md)
cover splash loading, initialization and buttons. Menu transitions, World entry,
physical input dispatch and gameplay still need integration.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
separate hosted results, local Windows checks and original comparisons.
The next session milestone remains loading one World, walking, changing Blocks,
and saving and reloading.
