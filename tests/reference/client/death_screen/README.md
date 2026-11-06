# Death screen component observations

The packet retains 72 ordered rows from two fresh JDK17 interpreter VMs using
the unchanged client JAR with SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Both captures complete with zero attempted network access, native-library
loads or writes. Class origins, selected method hashes, complete input/helper
fingerprints and bounded subprocess receipts remain checked.

The original observer invokes genuine CH, DA and KE constructors and a genuine
GR Session constructor. Its App is a declared prepared original `kq` receiver
created without executing its constructor. No game class is subclassed or
replaced. These inputs do not establish complete original App construction,
World loading or a graphical gameplay session.

Twelve profiles cover four signed sizes and three App/Session states. Each
profile records the CH initialization result, both ordered Buttons (IDs,
position, size, enabled/visible flags and complete UTF16 labels), nonpausing
result and three keyboard calls with a null App. A null App fails after both
Buttons have been appended. A null Session disables only Title menu. The
labels are the original literal `Respawn` and `Title menu` Strings.

`source-pins.json` binds the observed methods and supporting original classes.
`finite.json` holds only selected output rows; `provenance.json` binds those
rows to the two original captures. `test_packet.py` rejects damaged row order,
extent, provenance, input/helper declarations and bounded-process records in
ordinary and optimized Python.

The native fixture uses registered production App storage and independently
constructed genuine Session, World and Player owners. It does not run the
complete App constructor.
Additional authored controls cover constructor budget failure, repeated
initialization, retained old Buttons, null List, active admission, complete
native cleanup and a cold main-menu class. Those controls remain separate
from the 72 original component rows.

Windows Release and Linux Clang Debug with ASan/UBSan each pass all 369
native checks and all 72 selected original rows. Normal and optimized Python each pass 32 packet
admission/corruption controls. Its selected native inputs and authored resources
remain unchanged. These results do not qualify the separate loaded lifecycle.

Reproduce the original observations with local resources:

```text
python -B tests/reference/client/death_screen/capture_reference.py \
  --client-jar /private/b1.7.3.jar \
  --library-root /private/launcher/libraries \
  --output /private/new-death-screen-capture
```

Compare native without Java:

```text
python -B tests/reference/client/death_screen/run_native.py \
  --executable /private/mc_test_death_screen \
  --work-dir /private/death-screen-native
```

This packet does not compare CH drawing, Button actions, full input draining,
loaded death, respawn or original whole-frame pixels. Native screen and
coordinator integration require their own qualification.
