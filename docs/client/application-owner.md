# Minecraft class activation and Timer hack worker

Minecraft class activation and the `kg` worker are integrated startup
prerequisites. The combined source passes 393 Windows and 385 cases per sanitized
Linux compiler. The complete application constructor and normal startup remain
unfinished.

The current title/inspection frontend remains a component bootstrap. Its native
`running` and `captured` flags do not establish the roles of source volatile
fields `J` and `o`. The complete App layout adds exactly one atomic
storage location for each actual source field. Later source construction and
run methods must read and write those locations at their actual callsites.

## Class activation

The verified original Minecraft initializer allocates byte array `b` with
10,485,760 entries at PC2 and stores it at4. It allocates distinct 512-entry long
arrays `E` at10/store12 and `F` at18/store20. It then stores `G=0` at24, `H=0` at28
and `af=null` at32. Static `a` keeps its default null value. There is no source
CountDownLatch construction in this method.

The native class owner starts cold. Its mutex supplies the activation latch
and has no invented source field. First active use allocates the three
real zeroed arrays in order. A capacity or allocation failure retains all
earlier published arrays and records the actual allocation PC. Later active use
of this erroneous native class returns its latched cause without rerunning the
initializer. Native status categories do not claim Java exception identity.

The array budget counts native retained payload bytes. Its allocation limit
counts these three array allocations. Native class storage and synchronization
setup precede the source initializer and are reported separately. The default
16 MiB payload limit admits all three arrays. No Java heap-byte equivalence is
claimed. A successful occurrence release clears its passed pointer even when
another hold remains. Final release frees the arrays and native class owner.

This module does not implement later static `a` publication at constructor264,
static File selection, array updates in the run loop or emergency reserve
release. Those methods must use the same class namespace when implemented.

## Actual App lifetime and atomics

Native registration is mandatory before any atomic access. Registration
initializes the real default-false `J` and `o` and the native worker-hold count.
It does not credit a constructor store. In particular it does not move source
`J=true` from constructor138 to allocation or native setup.

Registration accepts a uniquely owned real App on the stack or heap. It does
not allocate an App, infer a heap owner or create a second App layout. The
caller must keep that storage alive until native retirement succeeds. Each
retained worker occurrence prevents retirement with `BUSY`. Retirement does not
write `J` or `o`, and retired storage cannot be registered again. All source
volatile operations use sequentially consistent C17 atomics.

## Worker construction and sleep

The actual `kg` constructor stores its App at2, calls `Thread(String)` at7,
sets daemon true at12 and starts at16. The native owner stores the same real
App and retains its registered native lifetime. It retains the supplied
immutable String as its native Thread-name role, records the actual daemon
policy and creates a real joinable C17 thread. A failed constructor retains its
completed fields and App/String occurrences until native retirement. Native
Thread setup is a bounded lower implementation, not a complete replacement for
all `java.lang.Thread` behavior.

The worker repeatedly loads actual `J` at `kg.run`4. When true, it sleeps for
2,147,483,647 milliseconds at13. A real native interrupt clears the pending
interrupt, follows the handler19 role and returns to the next `J` load. Setting
`J=false` alone does not wake a sleeping worker. Spurious condition wakes keep
the same sleep deadline and do not add a source `J` read or completed sleep.

Windows uses an SRW lock, a condition variable and relative wait intervals with
`GetTickCount64` elapsed accounting. Linux uses a pthread condition explicitly
configured for `CLOCK_MONOTONIC` and a fixed monotonic deadline per source sleep.
Neither implementation uses UTC deadlines, source wall-clock providers or
Timer's native clock context. No process clock or system setting is changed.
Host suspend/resume behavior, timer precision and general Thread
interoperability still need reference validation.

The Windows bridge follows Microsoft's
[condition-wait contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepconditionvariablesrw)
and [elapsed-tick contract](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount64).
The API documentation requires rechecking predicates after spurious wakes;
the implementation keeps its existing deadline through those wakes.

Native stop uses its own cancellation flag, signals the wait and joins the
actual handle. It never manufactures a source `J=false` or `o` store and never
counts native cancellation as `InterruptedException`. The joined owner remains
inspectable until retirement succeeds. This preserves `J=true` after an outer
constructor failure following138. Source daemon policy does not require
detaching a native thread that owns a real App borrow.

## Verification

The authored C test covers cold/ready statics, every allocation-limit prefix,
byte-limit prefixes, all array contents, distinct identities, repeated erroneous
use, four concurrent active-use callers and occurrence-consuming release. Its
real worker cases cover unregistered App rejection before atomic access,
retained null-name failure, one-shot construction, default-false exit, true
sleep, native spurious wakes, interrupt/reload behavior, source stop, independent
native cancellation and App retirement while worker holds remain.

The native `original` profile and authored Java observer describe the same five
normal class/worker projections. The original observer uses the unchanged
Minecraft class initializer and real `kg` instances on an explicitly prepared
receiver; it does not execute the Minecraft instance constructor. The native
profile contains five exact records. Fresh current Linux original runs match
all five; Windows retains its qualified original observations and matching
current native profile. The [reference package](../../tests/reference/client/application_owner/README.md)
retains its initial protocol plan; current passing results are recorded here.

Each combined compiler product passes 225 native contract checks. The capture
driver passes all 70 admission controls normally and with Python optimization,
including genuine fresh native output, altered semantic fields, failed process/
reader evidence, sanitizer stderr and forbidden ASCII line separators. Actual
source/header consumers rebuild coherently; Windows uses its default 1 MiB
stack and Linux uses ASan, UBSan and leak detection.

The prepared original receiver is the unchanged concrete `kq` subclass.
Minecraft itself is abstract. Unsafe allocation executes neither instance
constructor, and the observer invokes no `kq` virtual/frame methods. The
complete `kq` class/Code and actual load origin are additional authority
inputs; this is not a completed Minecraft or `kq` constructor result.
