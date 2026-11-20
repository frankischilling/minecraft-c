# Shared lighting class initialization

`capture_reference.py` records three rows from the unchanged, hash-pinned Beta
client JAR on JDK17. Two fresh processes initialize actual `u` with cold `ge`,
then with `ge.e(16)` already used. Both observe the same `u.a` identity on repeated
reads: a direct, zero-filled FloatBuffer with capacity/limit 16 and position 0;
the actual `ge.a` and `ge.b` Lists remain empty. The eager buffer is a distinct
identity.

The third process retains enough direct memory to leave 32 bytes of its 1 MiB
budget available. Actual `u.<clinit>` calls `ge.e(16)` and fails allocating its
64-byte backing with `OutOfMemoryError`; subsequent active use throws
`NoClassDefFoundError`. The observer verifies that failure reaches `u.<clinit>`
and that the completed `ge` List initialization survives. Network, listener,
write and native-library attempts are denied; process receipts and before/after
input hashes accompany each capture.

`run_native.py` compares those three rows with `mc_test_source_resources`.
Additional native assertions exercise allocation quotas 0..4, retained partial
`ge` initialization/backing allocation, terminal `u` failure, shared class-arena
buffer ownership, repeated active use without allocation, and failure at the
first `u.a` GL call with a NULL Display. Native quota failures are explicit
policy errors, distinct from Java allocation exceptions. The failure row
compares retained class-failure behavior, not exception-category equivalence.

These CPU fixtures establish class initialization and shared buffer lifetime.
They do not execute original `u.a` GL disables, render a World, or establish
terrain output. Source GL integration is exercised separately by the sky and
loaded-client controls.
