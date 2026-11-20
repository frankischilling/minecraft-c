# Single-player controller tick and reset

The finite fixture contains 60 state rows observed from the unchanged client
`os.c()V` and `os.a()V` bodies in the pinned Beta 1.7.3 JAR. It contains numeric
observations only. The JAR, classes and assets are not redistributed.

Each case constructs a genuine original `os`. The third group also invokes a
genuine original `yo` whose shared initialized flag is still false. The App
operand is the original `kq` class with Object-only allocation; its constructor
does not run. The observer authors only its actual `B` field. This measures the
selected OS bodies and disabled YO branch, not App construction, sound setup,
device playback, a complete client tick or a playable client.

The three groups use null App, null App.B and uninitialized YO. Each group has
ten float inputs: positive and negative zero, finite values, a subnormal,
infinities and distinct quiet/signaling NaN payloads. The observer checks every
other controller field, including inherited fields, around both operations.
It records the g store before each null failure and the reset's positive-zero
f/zero i stores. The process denies network, native-library links and writes;
its final receipt requires zero network and native-link attempts.

The exact bodies are 19 bytes for `os.c()V`, SHA256
`94149554a8f28f10cf028f3214d48879c781a4edd79761f812b8779906b246c1`,
and 11 bytes for `os.a()V`, SHA256
`8d00015a80cd210384f3937d4c4c79e43f5384496d2a6ddf759da1a12a7191c5`.
[Source pins](source-pins.json) retain the class and method identities.

To recapture with an existing JDK17 and the four pinned SDK metadata JARs:

```text
python -B tests/reference/client/controller_tick/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-controller-tick-capture
```

The runner requires a fresh output directory, verifies the original inputs
before and after execution, and writes bounded process receipts and a numeric
fixture. It makes no downloads and uses no native SDK binary.

The public `client.singleplayer_tick` CTest needs no JAR or JVM. Its native test
also checks actual SoundManager setup, disabled music, the positive countdown,
empty music selection, ordered playing queries, native and source failures,
retained per-invocation diagnostics, ordinary foreign holds and callback
reentry. Its recording backend is confined to the test. The production
controller calls the existing SoundManager on the actual borrowed App.B.
The replay runner requires successful native completion and compares all 60
rows with [the original observations](finite.json).
