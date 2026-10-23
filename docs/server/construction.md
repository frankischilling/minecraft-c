# Server default construction

The [Server owner](../../include/mc/server/runtime/construction.h) represents the
dedicated binary's `MinecraftServer()` constructor. Allocate one RAW identity,
then make one constructor attempt. Failed attempts preserve their completed
field prefix and cannot retry.

The constructor stores q=true, g=false and h=0, creates separate r and s list
owners, and creates k with two NULL tracker entries. The synchronized s wrapper
has its own backing list and mutex. All 18 declared instance fields are
inspectable; connection, properties, Worlds, player manager and console remain
NULL. Construction opens no files or sockets and performs no Random operation,
clock sample or game tick.

The original `bt` daemon only sleeps in a loop. Its native counterpart waits on
an owned condition until final release, then joins. It reads no game field.
This is an explicit adaptation of host lifetime, without a claim about JVM
thread bootstrap, daemon scheduling, interruption or timing. Full startup,
network listening, commands, ticking and source shutdown remain unfinished.

Typed retains protect the Server while a dedicated World holds F. That World
takes its hold only at the reached source store, after the base constructor and
Entity index complete. Native final release retires the inert thread and actual
children without invoking source shutdown or saving a World.

The direct allocation ceiling covers the owner and its constructed children,
excluding allocator overhead and host thread stacks. Native allocation/thread
failures have explicit diagnostics and source-prefix locations; Java OOM and
thread-failure behavior are unqualified. Focused tests cover raw defaults,
constructor ordering, allocation boundaries, child identities, failed-prefix
retention and balanced lifetime. See the
[dedicated constructor reference map](../reference/dedicated-world-construction.md).
