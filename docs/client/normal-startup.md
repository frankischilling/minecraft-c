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
resource lookups and color-array setters. It then constructs EntityRenderer and
its distinct held-item renderer, publishes the actual App field, updates the
loading title and initializes RenderManager. RenderManager retains its separate
held-item renderer and the real renderer/model map. Source fields publish only
after their constructors return. Startup then constructs the real statistics
writer from the live Session and game directory, publishes Minecraft.I and
installs its achievement-description callback. It invokes the startup logo,
creates the actual keyboard and mouse state, constructs MouseHelper and
publishes Minecraft.C at offset 380. It then runs Controllers.create383.
The admitted empty-device path continues through both GL diagnostics and setup.
It constructs the capability checker and publishes `S477`, binds the real
SoundManager to Settings, and completes nine texture-effect registrations.
The caller then constructs [RenderGlobal](render-global.md), publishes `g628`,
sets the viewport, constructs the particle renderer and publishes `j660`.
Startup constructs the real resource download thread, publishes `U676` and
starts it at `683`. The worker retains its App, File and output dependencies;
native retirement requests cancellation and joins before earlier owners retire.
It uses the original HTTP endpoint, scans local resources on the original
failure branch, and registers sounds through the real SoundManager pools.
The caller then performs the third GL diagnostic, `Post startup` at `693`.
The next unconnected caller operation is GuiIngame allocation at `697`.
Its independent constructor and the source menu owners exist, but GUI caller
publication, menu transitions and World entry remain unfinished.

A null Session fails in the genuine statistics constructor at `xi337/cj48`;
it cannot reach the logo or input setup. The successful Session profile starts
the statistics read worker without joining or servicing it during startup.

Resource loading uses the existing File, Pack, ImageIO and UTF-16 String owners.
It retains the actual Settings/Pack/Engine/Font identities rather than creating
replacement objects for each call. The ordinary imported-resource profile needs
`pack.png`, `font/default.png` and the three `misc/*color.png` resources at the
class-resource root. SDK icons live below `sdk/`. This namespace differs from
the importer's enclosing directory with its `jar/` child; the provider must be
rooted at the actual resource layout. The window controls retain two filesystem
providers: the import root for SDK icons, and its `jar/` child for class resources.
Both providers outlive their GUI and graphics users.

The default directory provider queries the real native home/profile and
environment. Windows applies the source APPDATA selection. A controlled window
test gives each child process its own APPDATA directory while preserving its
real home and all other inherited environment entries. Normal startup does not
use a temporary-directory or asset-directory fallback.

## Controllers and GL diagnostics

Controllers uses the shared SDK input owner and a real SDL joystick subsystem
reference. Its source-created flag follows successful construction; repeated
create returns without another discovery. Empty discovery completes. Nonempty
physical-device mapping stops at the explicit SDK Native12 frontier. The
official SDL HIDAPI and virtual joystick drivers are enabled; controls attach
and detach real virtual devices. Polling adds no event pump or FIFO removal.

The reached diagnostics call `Minecraft.c(String)` over the same Display. Each
invocation consumes exactly one glGetError result. Zero returns immediately;
nonzero maps GLU text and performs three fresh System.out reads before their
arguments and println calls. The counted PrintStream pipeline retains source
return separately from physical output and ordinary swallowed I/O failures.
See [the GL and output scope](display-sdk.md#startup-gl-setup-and-diagnostics).

## Sound, texture effects and scene constructors

SoundManager uses the actual Settings identity and its canonical options File.
Setup saves zero music/sound volumes before backend construction, restores the
values and saves again. The [typed FileWriter](../core/java-writer.md#typed-filewriterfile-bridge)
keeps those writes on the same File namespace and stream implementation.

The nine registration calls capture the live Engine before evaluating each
argument. In source order they register the constructor-owned lava and water
effects, then portal, compass, watch, flowing water, flowing lava and the two
fire orientations. Each call appends its actual effect and executes its first
update. Failed construction, appended occurrences and completed updates remain
separate retained effects; cleanup does not reverse their Random or field stores.
See [texture effects](texture-effects.md).

RenderGlobal receives the actual App and captured Engine. Its completed owner
defines the star/sky lists and optional query names before `g` publication.
The viewport reads the actual width and height independently. The particle
constructor receives the current nullable World and live Engine; normal startup
constructs its four Lists before publishing `j`. These steps do not enter a World
or execute a frame.

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
Application retirement detaches its RenderManager consumer before shared
renderer resources retire. Shared or externally held Font/renderers keep their
actual dependencies live through a BUSY result and retry.

Statistics retirement joins its actual worker before releasing its files,
Session or Registry. The Registry owns the installed callback occurrence and
keeps its App alive until that occurrence retires. Dropping the Minecraft
static occurrence alone cannot free an App still referenced by that callback.
Foreign or previously retired statistics owners are rejected before adoption.

A retained diagnostic owns its consumed query result, evaluated Strings and
captured PrintStream occurrences until native retirement. BUSY preserves those
occurrences for cleanup retry; it does not replay the source query or output.

The [MouseHelper and cursor](input.md) use the same Display/input namespace.
Native retirement releases the candidate's counted occurrence before graphics
and Display teardown. An external MouseHelper or cursor hold preserves its
actual Display lease and makes final teardown retryable.

Later scene owners retire before the registered texture effects and Engine.
External RenderGlobal, particle or effect occurrences return BUSY while keeping
their App/Engine/Display dependencies alive. Native list/query retirement and
context leases remain retryable. Completed source publications are not rolled
back by terminal cleanup.

## Validation and remaining work

The current increment passes 24 selected Windows cases: six on the production
successor and eighteen after a test-only triangle-observation correction.
All production code, headers, CMake and flags are identical across those two
snapshots. The normal Session performs 181 checks through `j660`, stopping at
`new cz664`. Typed Writer and Settings-save controls perform 582 and 202 checks;
the RenderGlobal device control performs 16,723 checks, including complete cleanup.
No successful case was rerun merely to change its source label.

Clang 18 passes 41 selected cases with ASan, UBSan and leak detection on the
production successor. That selection excludes the Windows default-directory
Session and RenderGlobal device profile. GCC qualification remains separate.
The [testing record](../testing.md#startup-scene-construction-and-typed-settings-save)
records source scopes and remaining platform checks. These results do not
establish original-JVM/visual correspondence, World entry or gameplay.

The earlier GL-diagnostic startup-tail source passes 47 selected Windows cases; nine Core
output cases retain their preceding Windows source scope. Clang 18 and GCC 13
each pass 41 CPU/protocol and twelve real-device cases with ASan, UBSan and
leak detection. Fresh Windows normal and real-GL-error Session profiles pass
103 and 109 checks through the cx470 boundary. Linux default-directory/Session
startup and original-JVM or visual correspondence are outside those results.
A separate Release headless build links the region-inspection utility with
client, sound, audio and testing disabled. It does not run the utility or
qualify server behavior. The following earlier results retain their own source
snapshots.

The [testing record](../testing.md#normal-startup-owner-qualification) separates
the earlier cohorts from the later native checks. Nine fresh Windows window
profiles exercise twelve actual Apps and pass 173,090 checks. The default and
shared-Font profiles reach the explicit offset-324 boundary. Nineteen separate
loading-screen drawing profiles pass 888 checks. These are native integration
results, without an original-game comparison.

The separate startup-logo callee passes five real Windows profiles and 414
checks over owners constructed by this genuine prefix. It performs actual
texture lookup, two draws, swap and complete retirement. Null-receiver and
floating-point failure profiles retain their reached effects. These prepared
calls do not credit the intervening statistics or callback calls. They have no
original-game or visual-parity result. The opt-in test requires the explicit
default-directory option; Windows isolates APPDATA, while POSIX uses the real
native home and remains unqualified for this profile.

The later Windows Session profile passes 92 checks through actual Stats, callback,
logo, keyboard/mouse and MouseHelper publication, stopping before Controllers383.
Its separate admission/null-argument controls pass 170 and 65 checks. These
results belong to the initial source snapshot. The final strict-warning
successor passes the genuine 92-check Session profile again after a minimal
input-unit/archive/test refresh. Both actual processes keep their source
bindings.
The later test record also retains failed-statistics/shared-Font cleanup,
five prepared logo profiles and cursor controls. Wider Linux qualification is
still recorded separately from the Windows default-directory profile.

Nonempty physical Controllers mapping and later GUI/menu
transitions, World/Player publication, frame/tick scheduling and the live game
loop remain unfinished. The default executable continues to use title/inspection
startup.
