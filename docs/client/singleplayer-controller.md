# Singleplayer controller construction and Player preparation

The canonical singleplayer controller implements the original `os` constructor
and selected World-entry methods. It creates a genuine LocalPlayer through the
inherited `ob.b(World)` body, performs the yaw preparation and supplies the two
actual empty World/final-Player hooks. Application World entry, mining, item use
and controller ticking remain separate work.

The native owner retains its client Entity classes and borrows their bound
Random profile, which must outlive those classes and the controller. Source
construction stores inherited `b=false` and the borrowed application, followed
by the original `-1`, positive-zero and integer-zero defaults. It does not
construct another Random or publish any application field.

Player creation allocates `dc` before loading the current application Session at
bytecode13, then reads the actual World provider at17 and its dimension at20.
It invokes the genuine LocalPlayer constructor at23. Each call reloads Session;
null application, World, provider and Session retain their reached failure
prefixes. Success returns one caller hold without spawn/reset, input assignment,
World insertion or application Player/view publication.

One failed allocated Player may remain owned for inspection. A second attempt
requires that slot to be cleared first. Clear releases only the controller's
hold, so independently retained RAW or failed Players remain valid. Clear and
native controller retirement allocate no diagnostic scratch. Ordinary operations
use heap error scratch to retain complete child diagnostics on the default
Windows stack.

`os.a(Player)` stores yaw `-180` at3 and preserves previous yaw and all other
fields. `os.a(World)` invokes the literal-return inherited World hook;
`ob.b(Player)` is also literal return. Their empty bodies are actual reference
behavior. They do not supply the separate singleplayer controller update body.

The authored native and allocator scenarios exercise ownership, failure prefixes,
live Session replacement and source fields. The optional Windows original
comparison constructs genuine original application/World/Player owners and
compares twenty records, including six interrupted prefixes. The comparison
checks original archive/class/Code origins, helper inputs, actual process
completion and native executable identity. Public tests use authored resources
and require no original archive or JVM.

This contract does not establish application World entry or a playable session.
Normal entry still requires the real LoadingScreen, audio, RenderGlobal,
Player persistence, World attachment and source-ordered application publication.
