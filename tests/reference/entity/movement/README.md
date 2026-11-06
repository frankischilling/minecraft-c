# Player movement reference

The ordinary `run_native.py` test needs the configured
`mc_test_player_movement` executable and Python. It uses authored resources and
real Registry, GAME World, Chunk, Session and LocalPlayer owners; no original
assets or JDK are required.

`compare.py` additionally requires the supplied client JAR, its four pinned
dependency JARs and matching explicit JDK17 compiler/runtime tools. Run it with
the same executable, `--client-jar`, `--libraries`, `--java`, `--javac`,
`--source-root`, `--tested-commit` and `--work-dir`. It creates fresh worlds and
compiles only the authored observation class. Actual game class origins are
checked. No native LWJGL library or display is used by this profile.

The comparison has21 exact original method/state records and12 separate JDK
Math.round component records. It covers the finite controls described in
[player-movement.md](../../../../docs/entity/player-movement.md). A real
post-constructor World Random.setSeed is an explicit input; generator Random
and production constructor entropy are not replaced. No full World array,
default-entropy startup, larger fall, statistics dispatch, Living travel or
complete gameplay parity follows from these records.

`source-pins.json` contains original JAR/class/Code hashes and the complete
paired73-class Block step-on inheritance map. The shared binary metadata parser
is `tests/reference/client/rendering/classfile.py`. Server entries are metadata
correspondence, not original server gameplay execution. Raw outputs, JVM class
files, worlds, assets and binaries belong only in the chosen private work area.

Run `test_controller.py` with normal Python and `python -O` to verify the
capture failure controls independently of native or original game execution.
