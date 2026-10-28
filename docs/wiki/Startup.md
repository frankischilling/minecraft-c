# Client startup

The native executable opens its title screen and a separate fixed-camera World
inspection. There is no playable client yet.

The [normal startup owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/normal-startup.md)
now connects Display, the default game directory, save manager, Settings,
selected Pack, RenderEngine, Font, color maps, EntityRenderer and RenderManager.
It then adopts the actual Stats writer/callback, invokes the logo, creates
keyboard/mouse state and publishes MouseHelper. The next unexecuted call is
Controllers.create at Minecraft offset 383.
The executable has not adopted a complete World-entry or gameplay loop.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#normal-startup-owner-qualification)
distinguish hosted builds, focused component tests and Windows startup
profiles. The genuine Session startup profile passes 92 checks through this
frontier. The next session milestone is loading one World,
walking, changing Blocks, saving and reloading.
