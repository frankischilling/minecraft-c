# Original CPU block rendering comparisons

Choose paths for the supplied client archive and the dependency directory.
Python 3 and JDK 17 are required. Standalone native compilation also uses GCC
with LTO support:

```powershell
$clientJar = 'C:\minecraft-reference\b1.7.3.jar'
$libraries = 'C:\minecraft-reference\libraries'
python tests/reference/client/rendering/prepare_libraries.py --output-dir "$libraries"
python tests/reference/client/rendering/compare.py --client-jar "$clientJar" --library-dir "$libraries"
```

Provisioning downloads four library JARs from the official URLs recorded in
[provenance.json](provenance.json), checks their SHA-256 values before writing,
and verifies existing files without replacing mismatches. The client archive
must be supplied separately. A directory already containing those four pinned
JARs can be used directly. No extracted textures, OpenGL context, native LWJGL
library or audio device is needed. `--cc`, `--java` and `--javac` select tool paths.

For configured CMake/MSVC or sanitized Clang/GCC products, supply all three
prebuilt probes. This mode does not invoke GCC:

```powershell
python tests/reference/client/rendering/compare.py --client-jar "$clientJar" --library-dir "$libraries" --faces-native FACE_PROBE_EXE --ao-native AO_PROBE_EXE --families-native FAMILY_PROBE_EXE
```

Build the three C files below as separate C17 executables linked to
`mc_client_world_mesher` and `mc_simulation`. `families.c` includes current
`mesher.c` to reach its internal CPU functions; the archive's separate face and
lighting objects satisfy the helper calls. Prebuilt mode hashes each executable
before and after execution. It also snapshots current sources. The caller's
build record must connect each executable to its sources.

The runner verifies archive, class, method Code and authored probe identities,
then preserves raw stdout/stderr, commands, tool versions, source hashes and a
JSON report in a fresh ignored output folder. `--output-dir` selects that parent
folder. Standalone mode compiles from its frozen source snapshot. Original
records must match their pinned hashes before native comparison can pass.
Every ordered integer/hex field is exact; there is no float tolerance. Only
the exact bootstrap lines `151 recipes` and `16 achievements` are excluded.
Normalized record hashes use ASCII, LF and a final LF.

[inputs.json](inputs.json) describes the authored fixtures. Both producers
contain fixed copies of these small input tables; their hashes bind those
copies to the description. No expected vertex payload supplies renderer inputs.

| Pair | Original entry | Compared records |
|---|---|---:|
| `FaceProbe.java` / `faces.c` | Six `cv` face helpers into `nw` | 108 XYZ/UV/ARGB vertices |
| `AoProbe.java` / `ambient_occlusion.c` | Direct `cv.a(Luu;IIIFFF)Z` | 108 vertex colors |
| `FamilyProbe.java` / `families.c` | `cv.b(Luu;III)Z` dispatcher | 534 vertices and 16 counts |

Original class bytes remain unchanged. Reflection selects authored Block bounds
and clears the Tessellator's active flag between fixtures, avoiding a GL draw.
AO/family receivers implement authored IBlockAccess grids. Family probes select
flat shading; the separate AO probe uses nonuniform light. Native family
fixtures use authored Chunk, Material and Registry receivers. They do not
construct or inject a GAME World.

The current rerun is recorded in [validation.json](validation.json). See
[World meshing](../../../../docs/client/world-meshing.md) for runtime behavior,
remaining families and the limits of these comparisons.
