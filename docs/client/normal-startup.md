# Normal client startup

The source startup owner implements the reached beginning of Beta 1.7.3's
`Minecraft.a()V`. It uses the same constructed App and real class/service
owners. The default executable still opens its title screen or fixed-camera
inspection. Normal World entry and a connected gameplay loop remain unfinished.

## Reached construction

The startup record performs the source `run` flag store, staged Display and
input setup, default game-directory selection, save-format manager creation,
Settings loading, Default/selected texture-pack construction, RenderEngine and
bitmap Font construction. It then performs the actual water, grass and foliage
resource lookups and color-array setters. Source fields publish only after
their constructors return. The current boundary is before EntityRenderer's
allocation at Minecraft offset 299.

Resource loading uses the existing File, Pack, ImageIO and UTF-16 String owners.
It retains the actual Settings/Pack/Engine/Font identities rather than creating
replacement objects for each call. The ordinary imported-resource profile needs
`pack.png`, `font/default.png` and the three `misc/*color.png` resources at the
class-resource root. SDK icons live below `sdk/`. This namespace differs from
the importer's enclosing directory with its `jar/` child; the provider must be
rooted at the actual resource layout.

The default directory provider queries the real native home/profile and
environment. Windows applies the source APPDATA selection. A controlled window
test gives each child process its own APPDATA directory while preserving its
real home and all other inherited environment entries. Normal startup does not
use a temporary-directory or asset-directory fallback.

## Failure and retirement

Raw and failed children remain in the startup record, distinct from completed
App fields. Errors retain the source caller, reached child and typed native
causes. An unfinished child remains an explicit frontier; the caller does not
turn it into a successful source return or a fabricated Java exception.

Retirement follows real dependencies. External Font, Engine and Pack holds
return BUSY while preserving their live Settings/Display storage. The outer
resource authority retires shared graphics resources with the same Display
still alive. An App does not retire somebody else's namespace. A consumed
child slot remains consumed even if cleanup reports an earlier device error.
Native worker cancellation and teardown do not synthesize source stop stores.

## Validation and remaining work

The [testing record](../testing.md#normal-startup-owner-qualification) separates
the selected native CTests, full builds and the retained partial Windows window
attempt. It does not claim full startup, original comparison or gameplay.
EntityRenderer/RenderManager, later render/audio owners, screen transitions,
World/Player publication, input/tick scheduling and the live frame loop still
need integration and end-to-end validation.
