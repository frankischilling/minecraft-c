# Submission failures before standard lighting

The 12 profiles invoke the unchanged whole `n.a(Lls;ID)I` on declared RAW
N/DK fields. Every profile fails before the U302 call. They cover zero or null
spatial arrays, negative and wrapped polling cursors, a null DK, a null dirty
List, null App or Settings after all ten polls, and a null camera with pass0
counter reset versus pass1 counter preservation.

The final profile enters the real rebuild at N95, stores the new render
distance, calls an old DK's `c()` at54, and fails in `b()` at13 on its declared
null pass array. Its actual chain is
`n.a(Lls;ID)I:95 -> n.a()V:54 -> dk.c()V:1 -> dk.b()V:13`.
The original concrete `kq` App and KV are allocated without their constructors;
no source App/Settings construction is claimed. No game method or returned
value is replaced, and no source failure is treated as ordinary false.

Read-only JDI observes a breakpoint at N26, immediately after each completed
R store, and the actual exception frame PCs. It does not change game fields,
methods, execution results or returns. The observer records the actual R,
dirty List identities, five counters and H. Each finite row appends the
observed store count and outer-to-inner source frame chain. Loaded N/DK Code
hashes must match the pinned archive's selected methods.

The capture checks the original JAR, complete selected class bytes, JDK17 and
SDK metadata. It fingerprints inputs and helper classes before and after.
The measured game denies network access, native library loads and writes;
the debugger's local JDWP transport is outside that guard. A public native
replay needs no JAR or JVM.

The native fixture calls `mc_client_render_global_submit_begin`, inspects its
retained state and compares every row exactly. Additional controls check that
U remains cold, pending calls block another submission and renderer retirement,
the retained terminal failure stays unchanged, and checked call/graph retirement
balances the actual N, graphics and Math holds. Null source App fields remain
null during RAW DK cleanup. The fixture initializes real prerequisite class
owners with authored resources and does not construct a World or Display.

These observations qualify the reached failure prefixes and cleanup. They do
not qualify U302, successful submission, rebuilding a complete grid, occlusion
queries, terrain drawing or a rendered frame. Those paths have separate device
controls.
