# Original ImageBufferDownload observations

[RrAlphaGrid.java](RrAlphaGrid.java) calls the unmodified client `rr` on authored
BufferedImages. It opens no window, reads no original images and makes no network
request. The client JAR, compiled classes and generated pixel files stay private.
See [validation.json](validation.json) for the exact binary/JDK pins and results.

For each TYPE_INT_ARGB and TYPE_4BYTE_ABGR input, 128 images cover all 65,536
alpha/channel pairs in each RGB channel. The top-left 32×16 rectangle receives
the grid; the rest is opaque `0xff123456`. The observer writes every output pixel
as a big-endian ARGB uint32 and checks that `rr.a` aliases that output's actual
DataBufferInt array. The two formats produced identical 262,144-pixel streams:

| Result per format | Value |
| --- | --- |
| Bytes | 1,048,576 |
| FNV-1a64, ARGB bytes in file order | `04e5e02bb77c37e9` |
| SHA-256 | `edad38f8044c8f2f7a15f0e3e082e0476d9e71f40ea08afd8f1ab18e5efcc55e` |

The native [image-buffer control](../../../client/render_engine/test_image_buffer.c)
uses the same authored grid and original FNV result. It processes the canonical
ARGB representation once; both observed original raster families have that same
expected output. The total original observation covers 524,288 full pixels.

Run with the pinned JDK and a separately supplied, hash-checked client JAR.
For example, on a shell whose Java classpath separator is `:`:

```sh
OUT=.local/image-buffer-observation
mkdir -p "$OUT"
javac -cp "$CLIENT_JAR" -d "$OUT" tests/reference/client/image_buffer/RrAlphaGrid.java
java -Djava.awt.headless=true -Dsun.java2d.trace=count -cp "$OUT:$CLIENT_JAR" RrAlphaGrid "$OUT"
```

Windows uses `;` as the classpath separator. Keep both generated pixel files in
the private output directory. The measured JDK trace uses IntArgb SrcOver MaskBlit, with the
additional ABGR conversion when required. These are finite CPU observations of
the pinned JDK/compositor. They do not qualify HTTP behavior, arbitrary ImageIO
providers, other JDKs, original rendering pixels or the complete client.

The processor's quantization was also checked against the pinned OpenJDK
[AlphaMath tables](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/java.desktop/share/native/libawt/java2d/loops/AlphaMath.c)
and [SrcOver loop](https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/java.desktop/share/native/libawt/java2d/loops/AlphaMacros.h).
The native implementation uses bounded integer arithmetic. A direct rounded
division differs from the reciprocal table at some half values, including the
alpha254 control. No OpenJDK implementation is copied into this observer.
