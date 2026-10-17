# Supplied binary audit

Issue: [#1](https://github.com/frankischilling/minecraft-c/issues/1).
Observed on Windows on 2026-10-17. These identifiers pin the local references;
they do not establish gameplay parity or runtime interoperability.

## Input identity

| Side | Local filename | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Client | `b1.7.3.jar` | 1465375 | `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c` |
| Server | `b1.7.3_server.jar` | 503100 | `033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d` |

The client's SHA-1 is `43db9b498cb67058d2e12d394e6507722e71bb45`.
Its hash and length match the client entry fetched from the official
[Beta 1.7.3 version metadata](https://piston-meta.mojang.com/v1/packages/44f6969326bd45aa00dcd3c4ca3a7c05ebb24c04/b1.7.3.json),
located through Mojang's [version index](https://piston-meta.mojang.com/mc/game/version_manifest_v2.json).
That metadata has no server download entry. The server's SHA-1 is
`2f90dc1cb5ca7e9d71786801b307390a67fcf954`; its publisher provenance has
not been independently verified against an official download manifest.

[manifest.json](manifest.json) contains hashes and aggregate counts, with no
asset contents, source, or class inventory. The client has 747 classes and 87
other files; the server has 444 classes and 6 other files. Every class header
has major version 49, minor version 0. The inspector read every archive entry
and verified ZIP CRCs. It checks class magic/version headers, not JVM validity.

The client manifest reports `Created-By: 1.6.0_22 (Sun Microsystems Inc.)` and
has no main class. The server reports `Created-By: 1.6.0` and
`Main-Class: net.minecraft.server.MinecraftServer`.

With no external class path, JDK 17 `jdeps --missing-deps` reports client
references in `org.lwjgl`, `org.lwjgl.input`, `org.lwjgl.openal`,
`org.lwjgl.opengl`, and `org.lwjgl.util.glu`; the server reports no missing
class dependencies. This static check does not test native libraries, audio
devices, assets, or reflection. The client's 831 per-entry manifest sections
all name files present in the archive. Matching official client bytes is the
available modification check; there is no equivalent official server hash
in the fetched metadata. Oracle documents dependency inspection in the
[JDK 17 jdeps reference](https://docs.oracle.com/en/java/javase/17/docs/specs/man/jdeps.html).

## Bytecode observations

Every row uses the side-specific SHA-256 above. Class names are obfuscated
identifiers within that JAR, not names shared between sides. Offsets below are
JVM instruction offsets within the named method, not source lines.

| Side | Class | Method and descriptor | Offsets | Observation |
| --- | --- | --- | --- | --- |
| Client | `net.minecraft.client.Minecraft` | `a()V` | 130, 132 | Loads `Minecraft Minecraft Beta 1.7.3`, then calls `Display.setTitle(Ljava/lang/String;)V`. |
| Server | `net.minecraft.server.MinecraftServer` | `c()Z` | 36, 38 | Loads `Starting minecraft server version Beta 1.7.3`, then calls `Logger.info(Ljava/lang/String;)V`. |
| Client | `nb` | `a(Lmp;)V` | 27, 29; 158, 160 | Each login branch loads integer 14 and calls `nz.<init>(Ljava/lang/String;I)V`. |
| Client | `nz` | `<init>(Ljava/lang/String;I)V` | 10, 11 | Stores the supplied integer in field `a:I`. |
| Client | `nz` | `a(Ljava/io/DataOutputStream;)V` | 2, 5 | Loads `a:I` and calls `DataOutputStream.writeInt(I)V`. |
| Server | `iv` | `a(Ljava/io/DataInputStream;)V` | 2, 5 | Calls `DataInputStream.readInt()I` and stores the result in `a:I`. |
| Server | `ee` | `a(Liv;)V` | 9, 12, 14 | Compares incoming `iv.a:I` with 14; equal branches to offset 42. |
| Server | `ee` | `a(Liv;)V` | 18, 21, 23; 27, 36 | A larger integer selects `Outdated server!`; a smaller integer selects `Outdated client!`. |
| Client | `ki` | `<clinit>()V` | 48-53 | Registers packet integer 1 with class `nz`. |
| Server | `gt` | `<clinit>()V` | 48-53 | Registers packet integer 1 with class `iv`. |

Inference: the supplied client and server agree on login protocol integer 14
and login packet integer 1. This is bytecode evidence of matching identifiers.
No connection, authentication attempt, or gameplay exchange was performed in
this audit; a four-way runtime interoperability matrix is still required.

## Signature limits

`jarsigner -verify -verbose -certs` exited 0 for each file. That exit code alone
does not establish publisher trust. The client has historical signature files
and signer metadata naming Markus Persson and Mojang. Temurin 17.0.17 reports
SHA1 and SHA1withDSA as disabled, a weak 1024-bit key, and treats the JAR as
unsigned. The server has no signature files and reports `jar is unsigned.`
No Java security policy was weakened. Oracle documents the treatment of
disabled algorithms in the [JDK 17 jarsigner reference](https://docs.oracle.com/en/java/javase/17/docs/specs/man/jarsigner.html).
Full command output remains in ignored `.local/logs/jarsigner-client.log` and
`.local/logs/jarsigner-server.log`.

## Local tools and reproduction

Reference tooling only: Eclipse Temurin OpenJDK `17.0.17+10` (64-bit HotSpot),
`javap 17.0.17`, Python `3.14.3`, and CFR `0.152`. Java is not a dependency of
the finished C executables. Oracle's [javap reference](https://docs.oracle.com/en/java/javase/17/docs/specs/man/javap.html)
documents `-c` for bytecode, `-s` for descriptors, and `-p` for private members.

CFR was downloaded from the author's [CFR release page](https://www.benf.org/other/cfr/)
to ignored `.local/tools/cfr-0.152.jar`. The release page documents whole-JAR
decompilation and `--outputdir`. This pinned artifact has SHA-256
`f686e8f3ded377d7bc87d216a90e9e9512df4156e75b06c655a16648ae8765b2`
and MD5 `8a85ada8cec494121246805a5562b82b`, matching the author's release MD5.
MD5 is recorded as a release cross-check, not a trust guarantee.

Commands below run from the repository root in PowerShell, after creating
`.local/tools`, `.local/logs`, and `.local/reference`:

```powershell
Invoke-WebRequest -Uri https://www.benf.org/other/cfr/cfr-0.152.jar -OutFile .local/tools/cfr-0.152.jar
java -version
javap -version
jdeps --version
python --version
java -jar .local/tools/cfr-0.152.jar --version
python scripts/reference/binaries/inspect_jars.py --client b1.7.3.jar --server b1.7.3_server.jar --output .local/reference/jar-inventory.json
java -jar .local/tools/cfr-0.152.jar b1.7.3.jar --outputdir .local/decompiled/client > .local/logs/cfr-client.log 2>&1
java -jar .local/tools/cfr-0.152.jar b1.7.3_server.jar --outputdir .local/decompiled/server > .local/logs/cfr-server.log 2>&1
javap -classpath b1.7.3.jar -c -s -p net.minecraft.client.Minecraft > .local/reference/client-Minecraft.javap
javap -classpath b1.7.3.jar -c -s -p nb nz ki > .local/reference/client-login.javap
javap -classpath b1.7.3_server.jar -c -s -p net.minecraft.server.MinecraftServer > .local/reference/server-MinecraftServer.javap
javap -classpath b1.7.3_server.jar -c -s -p ee iv > .local/reference/server-login.javap
javap -classpath b1.7.3_server.jar -c -s -p gt > .local/reference/server-packet-table.javap
jdeps --missing-deps b1.7.3.jar > .local/reference/client-missing-deps.txt 2>&1
jdeps --missing-deps b1.7.3_server.jar > .local/reference/server-missing-deps.txt 2>&1
jarsigner -verify -verbose -certs b1.7.3.jar > .local/logs/jarsigner-client.log 2>&1
jarsigner -verify -verbose -certs b1.7.3_server.jar > .local/logs/jarsigner-server.log 2>&1
```

Both CFR commands exited 0 and emitted 747 client Java files and 444 server
Java files. Each relative Java filename stem matches a class entry; the client
count includes 16 names containing `$`. Client `summary.txt` reports that two `com.jcraft.jorbis.Drft`
methods could not be fully structured; the server summary has no such entries.
Decompiler output is a navigation aid. The table above was checked against
`javap` output from the JARs. Decompiled Java and complete disassemblies remain
under ignored `.local/` and must not enter releases.

The inspector exits 77 and prints `UNAVAILABLE` if either local input is missing.
Invalid archives print `INVALID` and exit 2 without replacing an existing output.
It bounds archive size, expanded size, entry size/count, version groups, and
resource suffix summaries. It extracts no files and executes no JAR classes.
Synthetic fixtures exercised valid metadata, missing input, invalid ZIP,
invalid class headers, and preservation of prior output; all passed. The
supplied-input command above passed and produced the recorded inventory.
