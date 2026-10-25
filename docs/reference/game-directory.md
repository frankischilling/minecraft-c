# Game directory and startup settings contract

This record describes static original bytecode, selected JDK sources and the
new native implementation. Windows tests and the later hosted MSVC/GCC/Clang
run pass the specified native cases below. No original/default-directory
observation has been established. The
[earlier isolated persistence comparisons](game-settings.md#finite-native-comparisons)
pass an explicit scratch constructor directory; they do not measure ordinary
game-directory selection.

## Original Minecraft policy

The supplied client archive is SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
Its `net.minecraft.client.Minecraft` class is SHA-256
`db99df969e5f30d01ad4424a4d63d3b1f4a11832055dd291e65d476cfa4c38b3`.
Selected method Code bytes and resolved operands establish these calls:

| Method | Code SHA-256 | Contract |
| --- | --- | --- |
| `b()File`, 18 bytes | `b272a2fd232d6c70933c1c89670bc38232aa06e29e631ee8a1426f038a12c3f9` | Read static cached File at PC 0; call `a("minecraft")` at 8 only when null; publish at 11. |
| `a(String)File`, 268 bytes | `0f60f68af09c40e669cc1ba83a8984cda94694c314caf0c2f02016a7d934b5ee` | Read `System.getProperty("user.home", ".")` at 4, select the OS branch and child path, accept File.exists at 226 or require mkdirs at 233; otherwise throw at 265. No catch table. |
| `a()V` startup, 750 bytes | `80316ed4718ca0a300668801244d736ac48d85beacc82c09f1a4572241962493` | Display creation precedes cached directory selection at 160, save-manager construction at 184, GameSettings construction at 200, texture-pack/RenderEngine construction at 216/235 and FontRenderer construction at 256. |

The Linux/Unix and Solaris branches append `.minecraft/` to user.home. Windows
reads APPDATA at PC86 and uses it when nonnull, including an empty String;
otherwise it uses user.home. Its child is also `.minecraft/`. The macOS branch
uses `Library/Application Support/minecraft`; the other-OS branch uses
`minecraft/`. Only Windows and Linux have native implementations here.

The outer exists check does not require a directory. A regular file already
at the selected path is accepted. The later options File/load has its own
behavior. Security, property, environment and File failures can escape this
method; a fallback to a temporary or asset directory is not part of its body.

## JDK home and filename source

The retained installed profile is Temurin `17.0.17+10`, source commit
`3ae1373f8abb8add664985ab772915e681f56ea0`. Its selected files equal upstream
OpenJDK `jdk-17.0.17+10`, commit
`30ef840c3736270330fc0a26849c2456406facfe`. The source-acquisition manifest is
SHA-256 `51f025a035eccd25c51db29ec14df6acb5c745de80c00c09ef7f09343b886364`.
This connects source bytes to release metadata; it does not disassemble or
execute the installed native library.

- [Unix property initialization](https://github.com/openjdk/jdk17u/blob/30ef840c3736270330fc0a26849c2456406facfe/src/java.base/unix/native/libjava/java_props_md.c)
  uses `getpwuid(getuid())` and pw_dir, then `?` when lookup/copy yields null.
  It does not read HOME. Linux account bytes pass through the selected native
  charset and Java String conversion, including replacement behavior.
- [Windows property initialization](https://github.com/openjdk/jdk17u/blob/30ef840c3736270330fc0a26849c2456406facfe/src/java.base/windows/native/libjava/java_props_md.c)
  queries `FOLDERID_Profile` with `KF_FLAG_DONT_VERIFY`, then legacy
  `CSIDL_PROFILE`, then `C:\`. The wide result is UTF-16, not ACP text.
- [System property initialization](https://github.com/openjdk/jdk17u/blob/30ef840c3736270330fc0a26849c2456406facfe/src/java.base/share/classes/jdk/internal/util/SystemProps.java)
  supplies native user.home only when a VM property has not already supplied it.
  Empty and cleared properties are distinct inputs. Minecraft later reads the
  mutable property itself; native home discovery alone does not model that
  history or its security checks.
- [File.mkdirs](https://github.com/openjdk/jdk17u/blob/30ef840c3736270330fc0a26849c2456406facfe/src/java.base/share/classes/java/io/File.java)
  checks existence, tries mkdir, then gets a canonical File and recurses through
  its parent. Parent existence is checked only when parent mkdirs returns false.
  A canonicalization IOException returns false. This is not a generic recursive
  directory helper or a success-after-race repair.

The native owner uses caller-owned `getpwuid_r` buffers rather than the JDK's
borrowed `getpwuid` result and intermediate strdup. Its allocation effects are
not equivalent. It also avoids changing the process locale inside a getter;
general ParseLocale/native-charset correspondence remains unfinished. A
successful native account record with null pw_dir is refused before C-string
access, without claiming a Java result for that malformed record.

## Native ownership and App connection

The reviewed C candidate manifest is SHA-256
`2a4b56381dbb09bc8e80e982a5093e994d0a4741b0ddf1bd8c48670b3b2d2831`.
The public [GameDirectory interface](../../src/client/startup/game_directory.h)
owns a UTF-8 selected path and copies actual native operation state. Explicit
directory input bypasses native home selection; it is not a Java property
override. Successful construction retains no handles. Failed admission keeps
the previous output owner and any real filesystem prefix. Memory retirement
does not delete or save.

Linux canonicalization uses actual realpath queries and retained unresolved
suffixes; Windows uses `_wfullpath` and component FindFirstFile/FindClose work.
The original normalized path remains owned even when canonical creation uses
a different path. These calls do not by themselves prove general Java File
correspondence. The [native contract](../client/game-directory.md) lists
resource bounds and explicit unsupported path forms.

The App now constructs GameDirectory and one real file-backed GameSettings
after Display, before font/menu consumers. File-backed GameSettings defaults
and loads from that directory. A caught load failure leaves defaults and any
earlier field changes available to later consumers. No alternate directory or
replacement settings object is installed. Loaded GUI scale drives layout and
resize; an explicit native override, including zero, changes that owner after
load without saving. Initial anaglyph reaches font upload, lazy title texture
uploads and the FontRenderer palette.

This connection omits original save-manager initialization, general pack scan
and selection, AWT missing-texture construction, color-map publication,
SoundManager setup, input/cursor transitions and complete screen binding.
Original SoundManager initialization can write zeroed volumes before library
setup and save restored values afterward; failure need not restore them.
The native App has no audio owner and adds no corresponding startup/shutdown
save. Default resources and initial anaglyph use do not complete those bodies.

## Native verification and remaining scope

Windows MSVC x64 Debug passes 57 native GameDirectory checks and six independent
file checks. The separate graphics profile completes nine authored-resource
App cases and all eight exact decoded-pixel relations on an NVIDIA RTX 3060.
The App children exit naturally with status 0 and are reaped. Captures have an
actual 854-by-480 physical profile; no original image or expected field state
is supplied to the App.

Both three-case CTest profiles complete all seven validation stages and eleven
final audits. The frozen source contains 1,213 members, including 1,035
non-Markdown inputs. Retained output readback verifies raw/JUnit records,
capture trees and products without repeating a native run or introducing a
second pixel decoder. [Testing](../testing.md#startup-settings-and-game-directory)
keeps the earlier fixture compile failure and precise validation scope.

Windows default-directory testing is confined to a fresh supplied APPDATA
parent; Linux native-default creation is excluded. The later
[hosted run for `ea1aefc`](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841)
passes GameDirectory on all three compilers and startup on Linux, within
680 complete cases and 644-file formatting. Linux jobs request software OpenGL
under Xvfb; the graphics vendor is not recorded. The audit joins all 1,213
literal source bodies and 1,035 non-Markdown tested inputs. These inputs do not
establish an unmodified user.home, JVM property cache, original rendering or
original native-library default.

General filename/locale conversion, malformed bytes and arbitrary UTF-16,
Windows long/wildcard/trailing-dot paths, duplicate APPDATA names,
property/security/cache behavior and broader symlink/network/permission/race
cases remain required. Current refusals are unfinished implementation limits,
not approved compatibility exceptions. Full startup, audio, options UI,
physical input and original filesystem/pixel correspondence remain unqualified.
