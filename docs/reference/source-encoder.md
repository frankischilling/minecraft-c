# Source OutputStream encoding

The additive `mc_java_source_encoder` shares the existing Writer's encoding,
surrogate, storage and drain kernel. The legacy Writer layout, allocation
charge and physical committed-byte callback contract remain unchanged.

A source OutputStream write returns void. PrintStream can catch an I/O failure
after writing only a physical prefix and still return normally. StreamEncoder
then clears its byte buffer. The new callback records source return, swallowed
I/O and actual physical progress separately; it never reports an invented
physical byte count to a legacy sink. Unknown physical progress stays unknown.

A thrown source I/O outcome preserves the flipped buffer. Contradictory callback
outcomes are rejected without granting a normal return; inspection preserves
the rejected reported flags and physical facts. Buffer-only flush preserves a
pending high surrogate and does not flush the underlying output. Native
retirement neither flushes nor closes the source stream. BUSY preserves the
wrapper and borrowed context; other detach results consume the owner.

Focused MSVC, Clang 18 and GCC 13 qualification each passes fifteen selected
cases, including 91 new encoder checks. Fourteen Writer, logging, Settings,
storage and statistics regressions pass per compiler. The controls perform
real temporary-file writes before authored partial-output/error outcomes.
Their link maps verify one included Writer body and no second archive Writer
object. Linux uses ASan, UBSan and leak detection; Windows retains strict C17
diagnostics and default 1 MiB executable stacks.

This qualification uses a headless, client/audio-disabled target-only graph.
It does not establish a full client build, original JDK execution or a complete
System.out/PrintStream implementation. The latter remains required by nonzero
Minecraft GL diagnostics.
