# Player tick sleep and wake parents

The real Player tick now calls retained wake at the original `GS.w_` sites.
A missing Bed at 53 uses `(true,true,false)`; a valid daytime Bed at 73 uses
`(false,true,true)`. The timer increment, fresh World reads and actual `am`
coordinate/Block checks precede those calls. Checked child retirement must
finish before the inherited Living invocation at 111.

The [tick error](../../include/mc/entity/player/tick.h) owns the reached wake
call and an independent Registry hold. It preserves the first source failure
while reporting the current cleanup refusal separately. After a child is
consumed, its borrowed views end even if Registry retirement then refuses.
Retirement retries cleanup only; it does not repeat wake or later source work.
World dispatch passes the actual outer error-output availability, so an
internal scratch record cannot accept ownership for a caller with no output.
That refusal occurs only when wake is reached, after earlier timer effects.

World and client cleanup adopt this tagged ownership. The client refuses to
clear a record containing an owned Entity child, including a record whose
top-level flags are zero. The existing menu loop retains the record and graph
when cleanup refuses.

## Original observations and native comparison

The [new packet](../../tests/reference/entity/player_sleep_tick/README.md)
contains two unchanged-original VMs, each with 36 prepared Player calls.
Thirty-two stop at their actual, unresumed Living-call boundary111; four stop
at their first actual null exception. Each VM records 1,085 selected calls,
3,359 ordered events and 39 Code fingerprints. None of these original parent
calls returns. Explicit VM termination after rechecking suspended frames
supplies no source-return evidence. The two failed protocol preflights remain
separate from the accepted matrix.

The native bridge calls the whole real `mc_player_tick`. For the 32 boundary
rows it constructs a real App, while the original fixture has a null App.
That input difference is declared and unqualified. Comparison covers eight
sleep properties before and after each call: sleeping, timer, bed coordinate,
saved coordinate and four coordinate identity relations. Static source review
finds no later writes to these properties in the declared quiet fixture.
Position, motion, Random, animation, statistics and the private-bN tail remain
outside this original comparison. Profile7 retains its null World and must
fail in the actual native Living continuation.

The four pre111 faults keep the original null App input. They compare all
31 selected state properties, within-call Random/unselected preservation and
structured first-failure sites. The parent API has no typed Java exception
class for direct null guards; the comparison invents none. A retained wake
child reports its actual typed null classification.

Windows MSVC Release and Clang Debug Linux with ASan, UBSan and leak detection
pass all 36 rows and 1,625 scalar comparisons. The selected before/after state
coverage comprises 512 property slots for the 32 prefixes and 248 for the four
faults. These slot counts are separate from recursive scalar counts.
Thirty-one native whole calls return successfully; their counter and final
private-bN checks are authored continuation controls, with no original-return
qualification. Public readers reject 362 changed copies in both normal and
optimized Python; native transport controls reject 2,859 mutations per mode.

Native World controls exercise missing error output, ordinary no-output ticks,
real wake, occupancy failure and Registry cleanup refusal. A separate client
fixture passes 219 checks through the actual World failure record, no-overwrite
guard and staged cleanup. Its pin-limit fault and temporary mutable Block
notification flag are declared native inputs. It executes no Minecraft frame
or graphical gameplay.

```sh
ctest --test-dir build --output-on-failure -R '^(entity[.]player_sleep_tick|client[.]player_sleep_tick_cleanup|tools[.]player_sleep_tick_(packet|controls)_(normal|optimized))$'
```

Default-hostile CQ/FW, damage/NBT wake parents, remote sleep screens, loaded
night acceptance, whole original parent returns, original pixels/audio and
sustained gameplay FPS still need separate implementation or qualification.
