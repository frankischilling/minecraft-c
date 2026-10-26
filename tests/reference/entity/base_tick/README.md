# Base/environment update reference

The ordinary `run_native.py` driver runs `mc_test_player_base_tick` using real
constructed native owners and authored resources. It needs Python and the
configured executable; no original assets or JDK are required.

`compare.py` additionally needs the pinned supplied client JAR, four pinned
dependency JARs and matching explicit JDK17 compiler/runtime tools. It compiles
only the authored observer and concrete WorldAccess transport. Game classes
come from the unchanged original JAR and their runtime origins are checked.
No Unsafe, replacement game class, synthetic World/Player or native LWJGL
loading is involved in this profile.

The31 cases yield22 completed `dc.U` calls and nine interrupted prefixes. Exact
comparison covers230 records:31 state and199 sound/particle records. It includes
Random cache/state and both pool cursors. Native dependency status is not a
Java exception-class claim. See [the scope](../../../../docs/entity/base-environment-tick.md).

Supply `--executable`, `--client-jar`, `--libraries`, `--java`, `--javac`,
`--source-root`, `--tested-commit` and `--work-dir`. Runs create fresh private
worlds, captures and helper classes. Every subprocess has timeout and byte
limits. Source/runtime inputs and compiled helpers are hashed before and after
execution. `source-pins.json` records original class/Code metadata using the
shared rendering classfile parser.

Run `test_controller.py` normally and with `python -O` to test capture rejection
independently. Keep outputs, worlds, classes, assets and binaries private.
