# MovementInput

The C owner implements the complete original client `uo` MovementInput and
`lr` MovementInputFromOptions constructors and three methods. It uses the
actual [GameSettings](game-settings.md) named KeyBinding objects and their
current codes. [LocalPlayer forwarding](player-input.md) now attaches this same
owner and calls its key/reset methods. It does not construct or move a Player.

The public interface is
[movement_input.h](../../include/mc/client/input/movement_input.h). Original
JAR, class and method Code pins are retained in
[provenance.json](../../tests/reference/client/input/provenance.json).

## State and lifetime

Both classes start with positive-zero strafe/forward values and three false
boolean fields. The original field `uo.c` has no established gameplay role;
the C projection calls it `source_c` and preserves it through every method.
`set_fields` represents direct assignments to all five public fields. It
accepts float bit patterns without normalization or key changes.

The options constructor creates a separate ten-cell false key array, then
stores the supplied GameSettings identity. NULL settings is permitted: the
constructor, update and reset never dereference it. A later key event reaches
the original null-reference failure.

The options owner borrows GameSettings. Keep settings live through every input
hold, including attached Player fields. Settings mutations between calls remain
visible. The input now has a native retain counter: retirement clears the
caller's pointer and releases one hold; the final hold frees its array/header.
It performs no source reset or settings save. There is no automatic
settings-retirement guard. Owners remain live and exclusive throughout calls.

`read_state` copies the five fields and ten key cells. Its array identity and
settings pointer are borrowed diagnostics: keep their owners live while using
them. Each options owner has its own array even when several borrow the same
settings. Base MovementInput has no settings or key array.

## Key events

`mc_movement_input_key` executes dynamic `a(IZ)V`. The base class genuinely
returns without work. The options class compares the event code against six
named fields in this order:

| GameSettings field | Named index | Input cell | Default source code |
| --- | ---: | ---: | ---: |
| `m`, forward | 0 | 0 | 17 |
| `o`, back | 2 | 1 | 31 |
| `n`, left | 1 | 2 | 30 |
| `p`, right | 3 | 3 | 32 |
| `q`, jump | 4 | 4 | 57 |
| `v`, sneak | 9 | 5 | 42 |

Each comparison reads the current named reference and its current code through
the existing settings accessors. The settings `w` array is unused, including
when it is NULL or no longer contains those bindings. Aliased bindings retain
their identity and later raw code assignments are observed.

There is one key-cell store after all comparisons. When several bindings use
the same code, only the last matching cell changes. For example, assigning
one code to all six actions makes its event change only sneak. A missing
later binding fails before this store even if an earlier binding matched.
All signed int32 codes are admitted; platform key translation belongs to the
event caller.

Rebinding does not clear an already held cell. A release using the old code
does nothing when that code no longer matches. The caller can release using
the current binding or reach the actual reset method.

Source null-reference failures return `MC_MOVEMENT_INPUT_SOURCE_EXCEPTION`
with the original method PC and named-field index. A null settings read fails
at PC 7; null named bindings fail at PCs 10, 26, 42, 58, 74, 90. These failures leave
all public fields and key cells unchanged. Native invalid arguments and
allocation failures have separate statuses.

## Update and reset

`mc_movement_input_update` represents `a(Player)V`. Both original methods leave
the Player argument unread, so NULL is supported without a substitute Player.
The base body returns unchanged. The options body:

1. Stores positive zero to strafe and forward.
2. Adds 1 for forward, -1 for back, 1 for left and -1 for right in original order.
3. Copies cells 4 and 5 to jump and sneak.
4. When sneak is true, widens each axis to double, multiplies by double `0.3`,
   then narrows to float.

Opposing held directions cancel; diagonal axes are not normalized. Update
never reads settings or changes `source_c` or the key array. Floating
evaluation requires the project's strict binary32/binary64, nearest-even,
gradual-underflow profile.

`mc_movement_input_reset` represents `a()V`. The base body returns unchanged;
the options body clears all ten cells. It preserves the five public fields,
including previous jump/sneak values, until a later update overwrites the
appropriate fields.

## Verification and remaining integration

[Native tests](../../tests/client/input/test_movement_input.c) pass 1,214 checks
on Windows MSVC 19.51, Linux GCC 13.3 and Linux Clang 18.1.3. The Linux profiles
enable ASan, UBSan and leak checks. Cases cover all 64 six-key combinations,
live aliases/rebinding, duplicate precedence, every null named-field failure,
NULL settings, genuine base no-ops, output-bit preservation, independent
owners and retirement order.

The [original comparison](../../tests/reference/client/input/README.md) matches
all 857 records and 8,570 projected fields per compiler across 77 authored
scenarios. It invokes unchanged verified original `uo`/`lr` methods and passes
NULL to their unused Player parameter. Class-initialization logs require that
the only initialized original classes are `uo`, `lr`, `kv` and `qb`; no original
Player, World, graphics or audio setup is executed. Source/content pins and
the validation scope are in
[validation.json](../../tests/reference/client/input/validation.json).
The integrated root products also pass the native and original comparisons on
MSVC and sanitized Clang/GCC; those original runs use JDK17.

These checks establish this input-owner projection. Platform event routing,
mouse look/capture, Player construction, Living/Player ticks, movement and
camera integration remain separate work.
