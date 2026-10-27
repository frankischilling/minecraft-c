# Client startup

The native executable opens its title screen and a separate fixed-camera World
inspection. There is no playable client yet.

The [normal startup owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/normal-startup.md)
now connects Display, the default game directory, save manager, Settings,
selected Pack, RenderEngine, Font and color maps. Its source boundary remains
before EntityRenderer construction. The executable has not adopted a complete
World-entry or gameplay loop.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#normal-startup-owner-qualification)
distinguish full native builds, focused test results and the retained partial
Windows window attempt. The next session milestone is loading one World,
walking, changing Blocks, saving and reloading.
