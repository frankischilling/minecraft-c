# GameSettings persistence and effects

Minecraft Beta 1.7.3 stores client options in GameSettings (`kv`) and individual key bindings in `qb`. The verified class has 40 declared fields: 36 instance fields and four static label arrays. Constructor defaults, file parsing and saving are separate operations from the effectful option setters.

The authority is the supplied client archive, SHA-256 `af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`. Its unchanged `kv.class` and `qb.class` hashes are `fd62ebcc36e2f2a1428cb41b2f1f8ebd86fee9855ec1b7f7055ba307d6577102` and `4c1524aeb93b82f91517c5ed5a3dcce02c1b110a9d65b3db7089cfc8a37cd7aa`. Bytecode establishes the field and method contracts below. Controlled execution of the original constructors and load/save bodies supplies the finite persistence observations. It does not qualify native persistence, effectful setters or the complete client.

## Fields and defaults

These are the 15 saved setting keys, in the original save order. Serialized spelling is preserved, including `skin` and `advancedOpengl`.

| Saved key | Source field | Role | Constructor default |
| --- | --- | --- | --- |
| `music` | `a:F` | Music volume | `1.0f` |
| `sound` | `b:F` | Sound volume | `1.0f` |
| `invertYMouse` | `d:Z` | Invert mouse | `false` |
| `mouseSensitivity` | `c:F` | Mouse sensitivity | `0.5f` |
| `viewDistance` | `e:I` | Render-distance selection | `0` |
| `guiScale` | `I:I` | GUI-scale selection | `0` |
| `bobView` | `f:Z` | View bobbing | `true` |
| `anaglyph3d` | `g:Z` | Anaglyph | `false` |
| `advancedOpengl` | `h:Z` | Advanced OpenGL | `false` |
| `fpsLimit` | `i:I` | Performance-mode selection | `1` |
| `difficulty` | `y:I` | Difficulty selection | `2` |
| `fancyGraphics` | `j:Z` | Fancy graphics | `true` |
| `ao` | `k:Z` | Ambient occlusion | `true` |
| `skin` | `l:String` | Selected texture-pack name | `"Default"` |
| `lastServer` | `C:String` | Last server text | Empty string |

`skin` selects a texture pack through the original texture-pack owner (`ik`); it is not a player-skin URL. Loading does not clamp these values to UI ranges. This class has no saved fullscreen, language, VSync or modern numeric FPS option. Fullscreen belongs to separate Minecraft/Display state.

The remaining 21 instance fields complete the instance inventory:

| Source fields | Default and role |
| --- | --- |
| `m` through `v` | Ten distinct owned `qb` key bindings, described below |
| `w:qb[]` | An array of references to those same ten owners |
| `x:Minecraft` | Null, or the supplied Minecraft reference |
| `N:File` | Null, or a new `options.txt` child of the supplied directory |
| `z:Z` | `false`; hide GUI |
| `A:Z` | `false`; third-person view |
| `B:Z` | `false`; debug information |
| `D:Z` | `false`; retained alias for the flag gating the located mouse-wheel adjustment of `G` |
| `E:Z` | `false`; smooth camera |
| `F:Z` | `false`; the inspected renderer uses it for independent debug-camera orientation |
| `G:F` | `1.0f`; retained alias for the wheel-adjusted value |
| `H:F` | `1.0f`; retained alias, with no located external consumer in the retained instruction inventory |

The evidence does not assign guessed semantic names to `D`, `G` or `H`. The four static `String[]` fields `J`, `K`, `L` and `M` contain the distance, difficulty, GUI-scale and performance label keys. Their translated orders are Far/Normal/Short/Tiny; Peaceful/Easy/Normal/Hard; Auto/Small/Normal/Large; and Max FPS/Balanced/Power saver.

Each `qb` contains only its action-name string `a` and integer key code `b`. It does not contain later-version pressed-state or press-count fields. Bindings are constructed in named-field order `m,n,o,p,q,r,s,t,u,v`; their public array and save order differ:

| Array index | Named owner | Action name | Default legacy LWJGL code | Creation ordinal |
| --- | --- | --- | ---: | ---: |
| 0 | `m` | `key.forward` | 17 | 0 |
| 1 | `n` | `key.left` | 30 | 1 |
| 2 | `o` | `key.back` | 31 | 2 |
| 3 | `p` | `key.right` | 32 | 3 |
| 4 | `q` | `key.jump` | 57 | 4 |
| 5 | `v` | `key.sneak` | 42 | 9 |
| 6 | `s` | `key.drop` | 16 | 6 |
| 7 | `r` | `key.inventory` | 18 | 5 |
| 8 | `t` | `key.chat` | 20 | 7 |
| 9 | `u` | `key.fog` | 33 | 8 |

The file prefix is `key_`, so the first binding line is named `key_key.forward`. Load visits the current array in order without stopping after a match or deduplicating references. Binding setters, labels and save also read the current array. The observations retain named-to-array identity through reload/save and show distinct binding and array owners after a fresh construction. They do not replace the public array or install arranged field values.

## Construction and load

Both constructors establish the same defaults. The no-argument constructor leaves `x` and `N` null and does not load a file. The file-backed constructor stores its Minecraft argument, constructs `new File(directory,"options.txt")` and invokes load on the receiver. It does not create the directory. The genuine `kv(null, scratchDirectory)` receiver can therefore exercise this persistence cut without a fabricated Minecraft object, audio owner or GL context.

Selected bytecode anchors identify the reached operations:

| Method | Code bytes | PCs |
| --- | ---: | --- |
| `kv.<init>()V` | 356 | Default stores through 352; return 355 |
| `kv.<init>(Minecraft,File)V` | 379 | Owner 357; options File 371; virtual load 375 |
| `kv.a()V` load | 504 | File existence 4; FileReader 23; readLine 34; split 45; EOF close 484 |
| `kv.a(String)F` float wrapper | 27 | Exact `true` returns 1 at 9; exact `false` returns 0 at 20; Float.parseFloat at 23 |
| `kv.b()V` save | 495 | FileWriter 12; PrintWriter 15; setting/binding output through 465; close 475 |
| `qb.<init>(String,int)V` | 15 | Name store 6; code store 11 |

Load uses `FileReader` and `BufferedReader`, with the selected runtime's default charset. Lines end at CR, LF or CRLF; an unterminated final line is still a line. There is no GameSettings-specific charset declaration or BOM removal. Malformed-byte replacement belongs to the library decoder, not to a strict UTF-8 parser invented for this file.

Each decoded line uses `split(":")` with Java's default zero limit. Interior empty fields survive; trailing empty fields disappear. Keys and values are not trimmed. There is no comment syntax, escaping, case folding, colon rejoin or preservation of unknown keys. Unknown keys are ignored. Recognized duplicates apply in encounter order, and later malformed values leave the preceding value in place. A per-line parse exception prints `Skipping bad option: ` followed by the decoded original line, then continues with later lines.

Boolean values are true only for the exact lowercase string `true`. Other present spellings become false. The three float fields first recognize exact raw `true` and `false`, then use Float.parseFloat. Numeric whitespace accepted by that parser does not make ` true ` a valid wrapper value. Signed zero, NaN, infinities and values outside slider ranges are admitted. Integer parsing is decimal, permits an ASCII sign, checks the signed 32-bit range and does not trim. Its digit recognition uses `Character.digit` on UTF-16 units; it is not limited to ASCII digits.

Missing values and extra colons matter. `skin:` has no surviving value and is a bad option; `skin::x` stores an empty texture-pack name. `lastServer:` leaves the previous value because that key checks the split length. `lastServer:host:port` stores only `host`. A colon-only line loses all split fields and is caught as a bad option.

The per-line exception handler covers PCs `[42,451)`; its handler starts at 454 and then resumes the line-reading loop. Outer load handlers cover `[0,10)` and `[11,487)`, print `Failed to load options` plus a stack trace and return normally with the state reached so far. An absent file returns without creating it. The explicit reader close at 484 is reached at EOF; there is no source finally block closing every failing path. Those handlers catch `Exception`, not every `Throwable`. Mid-read failure and actual source stream-close reachability were not measured by these profiles.

## Saving and failure behavior

Save opens a non-append FileWriter, so opening can create or truncate the file before later formatting or writes. It creates no parent directory, temporary transaction, backup or rollback. It writes the 15 keys above, then one line per current key binding, then closes. A normal constructor-owned array yields 25 lines, with a platform line separator after the final line. Unknown input keys are discarded.

Floats are formatted by the selected Java runtime's string conversion; integers use signed decimal and booleans use lowercase text. Strings are appended literally, without escaping colons or newlines. A Java null string would append `null`, but public-field arrangements of null or embedded newlines were not part of these captures.

The writer uses the runtime default charset and PrintWriter's line separator. PrintWriter can swallow ordinary write, newline and close IOExceptions into its trouble flag; GameSettings never calls `checkError`. A normal save return therefore does not prove complete I/O, even when no save-failure message appears. The outer save handler covers `[0,478)`, catches escaping Exceptions at 481, prints `Failed to save options` and a stack trace, then returns. It also has no source finally block. Actual saved bytes and failure prefixes must remain observable rather than being converted into transactional success or unconditional propagated failure.

Reload identity is not a source guarantee. All three measured profiles loaded an empty pack name from the colon case, saved `skin:`, then restored the fresh receiver's `"Default"` pack name after that line failed to load. Encoding replacement and text conversion can also lose information. Each stage must be compared on its own terms, with its actual raw file; a roundtrip-equality assertion would reject an observed source behavior.

## Three controlled original profiles

Each profile ran the same 16-case catalog in fresh JVMs. Every case recorded nine JSONL rows containing four complete snapshots: genuine construction, explicit reload, save, and a fresh file-backed construction from the actual resulting file/directory/absence. Each profile retained 144 rows, 64 snapshots and 2,560 declared-field records, including float bits, counted UTF-16 strings, all named/array binding aliases, input edits, raw files, diagnostics and original class-resource origins.

| Profile | Actual runtime | Default charset / `file.encoding` | `native.encoding` | Line separator |
| --- | --- | --- | --- | --- |
| Windows default | Temurin 17.0.17+10 | windows-1252 / Cp1252 | Cp1252 | CRLF |
| Windows explicit UTF-8 | Temurin 17.0.17+10 with `-Dfile.encoding=UTF-8` | UTF-8 / UTF-8 | Cp1252 | CRLF |
| Linux default | Ubuntu 17.0.20.1+1-1-24.04-Ubuntu | UTF-8 / UTF-8 | UTF-8 | LF |

The common catalog is an input description, not a promise of identical bytes across default charsets. Its `default` form decodes authored UTF-8 text in the helper, then encodes it with the measured default charset before the original call; the exact resulting bytes and representability result are recorded. Its `raw` form supplies literal bytes. The Unicode default-encoding case recorded replacement and failed representability under Windows-1252, while both UTF-8 profiles retained those authored characters and parsed the exercised Unicode integer digits. The Windows UTF-8 run remains an explicitly selected Windows profile; it is not Linux runtime evidence. No general equivalence of the two JDK updates or of all profile states/files is asserted.

The cases include every saved key and binding, duplicate/bad lines, booleans, float specials and grammar, integer limits and Unicode digits, colon/empty values, raw line endings, unknown keys, missing/empty files, BOM/malformed bytes, an owned `options.txt` directory and a no-argument null File. The directory case records three caught load failures and one caught save failure; the null-file case records one caught explicit-load failure and one caught save failure. These are successful measurements of failure behavior, not successful file operations. No late-write or mid-read fault was injected.

All 16 processes in each profile exited naturally with status 0, were reaped and left no owned child tree. Windows used the retained Job owner. Linux used the retained PIDFD/subreaper owner, with 16 actual kernel `CLD_EXITED/0` results and 16 independently closed PIDFDs. Linux recorded the same canonical boot ID before child launch and after closure/final audits. Complete prepared/source/runtime pins and backups were retained. Loader/helper/process cleanup remains distinct from unobserved original reader/writer-close reachability.

The three retained report SHA-256 values are `14f3b45720baa0d5021e83423ab4005e14d054bd6ae9d5a54544393c59c34c9e`, `5bf06e082ac44961e5714f2ffbcb13f90166fed7b282c5302606606dadf909e5` and `24e6bfd73194905604cf66ee211e71be90af562cb37aa8627da3fbbfcca19794`, in table order. These observations used the unchanged original archive and one helper-only application class directory; no fake game classes, effectful setter calls, input system or rendering body ran.

## Setters and unfinished effects

Load/save never read the stored Minecraft owner and do not call option setters. They can be measured with null `x` without claiming the following effects are implemented:

| Operation | Source order and effect |
| --- | --- |
| Music or sound float setter | Store the field, then call the current real SoundManager (`yo`). It has no direct save instruction, but audio initialization can temporarily zero both volumes, save, create the sound system, restore volumes and save again. |
| Sensitivity float setter | Store the supplied float without clamping or a direct save. |
| Invert mouse or bobbing | Toggle the field, then save. |
| Distance, GUI scale or difficulty | Java wrapping addition and `& 3`, then save. |
| Performance mode | Java `(old + delta + 3) % 3`, including possible negative remainder, then save. |
| Advanced OpenGL, graphics or ambient occlusion | Toggle the field, call the real RenderGlobal (`n`) rebuild, then save only if the callback returns. |
| Anaglyph | Toggle the field, call the real RenderEngine (`ji`) texture refresh, then save only if it returns. |
| Key binding by index | Write the current array element's code, then save; no duplicate-code resolution. |

There is no rollback or catch around those setter callbacks. An absent receiver or real callback failure can leave the preceding mutation while preventing the later save. A null/nonfloat option is ignored by the float setter; the discrete setter still saves for nonmatching options. The original label paths use translation tables, direct array indexing, Java float-to-int conversions and LWJGL key-name lookup. Invalid loaded indices can fail during labeling even though load accepted them.

Real audio setup, renderer/texture rebuilds, platform key translation and physical key-name lookup remain unfinished dependencies of the client settings path. No-op effect receivers would not implement those bodies. The finite original persistence results above do not establish native persistence, menu setter behavior or whole-client correspondence.
