# LocalPlayer hurt observations

The packet captures 28 calls across 22 profiles to the unchanged original
`dc.a(sn,int)`, inherited `gs.a(sn,int)` and `ls.a(sn,int)` chain. The supplied
client JAR has SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Two ordinary JDK17 interpreter VMs agree on every selected deterministic
pre/post field. Each VM checks real class origins and the pinned class/method
bytes. Both complete with zero attempted network access, native loading or
rejected writes. Statistics files remain in the isolated work directory.

The observer uses real `kq`, `xi`, `fd`, `dc`, `ix` and `iz` constructors.
No game classes are subclassed or replaced. A `pm` sound observer records the
actual original sound arguments. These are prepared component operands; the
Player is not published into loaded Chunks. This is not a graphical session.

`profiles.json` declares the named profiles, seeds and values. `profiles.tsv`
is the exact native/Java input transport. Its columns are profile, health Y,
immunity by, prior damage au, armor remainder bO, Item ID, damage, mode,
number of calls and damage amounts. The three seeds are actor 38, World 2468
and shared Math 0. Actor position is `(8.5,200.5,8.5)`, yaw 30 and velocity
`(.25,-.375,-.125)`. Health animation and other prepared fields appear in the
observer. Calls within each profile reuse the same objects without repairs.

Profiles cover ordinary damage, repeated immunity and delta damage, the exact
half-immunity boundary, remote World, dead Player, zero damage, full/worn/breaking
armor, a nonarmor slot, armor remainder, nullable Inventory/armor array/App/
Writer/World/Random, invalid Item ID and sound callback failures. One callback
sets Player.World null after its sound; another throws. The low-damage immunity
case with null Random returns before dereferencing Random.

Each row retains the full typed `sn`/`ls`/`gs`/`dc` instance-field projection,
bounds, all MouseFilter fields, Inventory arrays and Stack, Player/World/Math
Random state including Gaussian storage, World fields/counts, statistic maps,
alias observations and cumulative sound arguments. Native replay compares
20,872 scalar values and passes 529 native ownership/state assertions.

Four Player fields have independent constructor values: Entity identity
`sn.aD` and randomized `ls.F`, `ls.G`, `ls.aj`. World `fd.I` is construction
time; `fd.Q` and `fd.g` come from independent construction state. Each call
must preserve its own incoming value for all seven fields. Their raw words
remain in the packet, but cross-process equality is not required for them.
No other before/after fields are omitted from the native comparison.

The fatal profile records its original state inside the actual hurt sound
callback, immediately before the reached death continuation. Original complete
death returns true and is retained under `completion`; native stops at the
explicit unimplemented Player death entry. Only that fatal prefix is compared.
The throwing callback produces original `IllegalStateException` and native
World IO refusal; their full reached states match, while causes stay distinct.

Windows Release native replay passes all 28 rows. Full Player death, nonnull
attackers, sleep/wake, arbitrary armor arrangements, sustained gameplay, Linux
original capture and FPS remain outside this packet's qualification.

Reproduce the original capture with local supplied resources:

```powershell
python tests/reference/entity/player_hurt/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/player-hurt-original
```

The output retains two bounded VM captures, input/tool/helper fingerprints,
real class binding receipts, separate isolated work directories, two finite
observation files and publication metadata. Compare native without Java:

```powershell
python tests/reference/entity/player_hurt/run_native.py `
  --executable build-windows/mc_test_player_hurt_original.exe `
  --work-dir build-windows/player-hurt-contracts
```

`test_packet.py` runs the same comparison against authored corruption controls.
CTest includes normal and optimized Python reader controls and the native
replay. Java, JARs, extracted assets and classes are not distributed.
