# Achievement initialization

The [achievement interface](../../include/mc/statistics/achievements/achievements.h)
shares the Block, Item, statistics, Translation and key-loader lifetime.
The original owner `ep/cy` creates a distinct list and runs all sixteen
construction, flag, registration and named-publication chains in order.
Each typed constructor creates a real [ItemStack](../item/stack.md) and reads
the current Item or Block ID. It does not perform an item-provider lookup or
apply stack-size or durability rules to that icon.

The Stack constructor wraps the raw achievement ID with Java int addition,
creates a fresh UTF-16 `achievement.` key and calls plain Translation. It
constructs the inherited statistic with the live integer formatter, stores
the icon, then creates a second fresh key ending in `.desc`. Description,
coordinates, live coordinate bounds and parent are written in source order.
Null suffixes append `null`; embedded NULs and surrogate units remain String
data. Client callback storage remains separate from inherited fields.
Later description/UI callbacks and achievement gameplay updates are unfinished.

The first achievement sets the inherited independent flag. The rail and pig
achievements set their own special flag. Registration calls the base operation
before appending to the achievement list: duplicate rejection, base-list/map
publication and actual key loading retain their original order. Achievement
registration does not append to the general-stat list. Class failure retains
completed writes and the first cause; public reentry reports BUSY.

After named publication, initialization captures process stdout and prints the
live achievement-list size with the actual suffix. The reached default ASCII
profile writes CRLF on Windows and LF on Linux. Ordinary OS write failures are
suppressed with a retained native diagnostic, like the reached PrintStream
error category. Linux preserves the caller thread's signal mask and preexisting
pending SIGPIPE. System.setOut replacement, arbitrary Unicode encoding and
Java thread-interruption behavior are outside this private output interface.

Entry order matters:

| Entry, with resources supplied | Current retained behavior |
| --- | --- |
| Statistics | Recursive Block/Item work completes recipes and smelting. Statistics publishes C, completes all sixteen achievements and clears G/H. Ordinary class initialization returns; C/D/E/F remain published. |
| Block | Nested statistics publishes C, completes all sixteen achievements and clears G/H. Item and Block callbacks then publish E/F, set G/H and build D from the completed managers. Ordinary class initialization returns. |
| Item | Named Items are still NULL when the first achievement reads its icon. A real RAW ItemStack exists before the Item constructor dereferences NULL at PC2. C remains published; statistics retain failure at624 and achievements at23. E/F are NULL and G/H false. |
| Crafting | All sixteen achievements complete, but the recursive recipe-stat helper reads the still-NULL crafting singleton at PC24. The pending manager is not published. |
| Smelting | All sixteen achievements and the crafting manager complete. The recursive recipe-stat helper reads the still-NULL smelting singleton at PC75. |

The current [bootstrap fixtures](../../tests/reference/statistics/bootstrap/provenance.json)
contain thirty captures across both original sides, all five entries and
interpreted, tier-1 and default Temurin 17.0.17+10 modes. Modes agree within
each side/entry. The
[observer](../../tests/reference/statistics/ArrayPrefixController.java) stops
after ordinary `Class.forName` return for Statistics and Block, or at the actual
original NULL dereferences at PC2/24/75. It checks the original loader,
bytecode and caller stack. Every debuggee process uses requested debugger
termination, including the two routes whose class initialization returned.
Released mirrors and reaped children do not establish natural process
completion. The fixtures cover the published map, arrays and lists,
achievement bounds/named/parent references, special flags, icon identity and
four real ItemStack fields.
They omit resource text and unpublished partial heap objects. Raw original
console bytes are retained; only the separately identified OS line separator
is adapted by the [native comparison](../../tests/tools/statistics/compare_arrays.py).

The eighteen [achievement-checkpoint captures](../../tests/reference/statistics/achievements/provenance.json)
remain historical evidence at recipe d21 or ItemStack PC2. They are unchanged,
as are the earlier array fixtures. Thirty-six replay captures using the
successor observer match both historical profiles byte for byte; d21 is not
the current implementation boundary. The completed recipe catalogs and
entry-specific singleton state are described in [crafting and smelting](../item/recipes.md).

The independent [PrintStream observer](../../tests/reference/java/PrintlnObservation.java)
records actual bytes and checkError results for four ASCII payloads, including
NUL, and two ordinary IOException streams. Its three-mode
[capture](../../tests/reference/java/println/provenance.json) is reproducible
with `scripts/reference/java/capture_strings.py println`. The native
[OS-handle controls](../../tests/tools/platform/test_stdout.py) exercise closed
stdout and broken pipes, plus Linux pending-signal and mask preservation.
They do not force native partial writes or claim identical Java/native fault
positions.

The [native ownership unit](../../tests/statistics/achievements/test_achievements.c)
checks actual registration/resource callbacks, retained READY and RAW Stack holds
across class retirement, entry-specific failure caches, nine natural bootstrap
budgets and a direct constructor partial prefix. Its isolated method fixtures
explicitly model INITIALIZING classes; they do not manufacture successful
bootstrap. At the achievement checkpoint, all 26 selected Windows and 28 cases
per Linux compiler passed, including 7,208 achievement and 7,598,064 array
checks. All six original numeric graphs and actual console output matched on
MSVC/GCC/Clang; Linux used
ASan/UBSan and leak detection.

The recipe checkpoint passes 30 selected Windows cases and 32 per Linux
compiler, including 32,892 achievement checks and all ten current numeric
graph/catalog comparisons plus both Stack-value projections. MSVC, GCC and
Clang agree; Linux uses ASan/UBSan and leak detection. Runtime description/UI callbacks, achievement gameplay, World
integration, population, synchronous mutations and gameplay persistence remain
unfinished.
