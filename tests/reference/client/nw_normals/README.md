# Tessellator normal packing

The authored probe invokes unchanged `nw.b(FFF)V` on the genuine NW singleton.
Its 17 input triples include signed zero, axis endpoints, fractions, nonunit
values, NaNs, infinities, subnormals and values that saturate Java integer
conversion. The normal method stores `n=true`, multiplies X by128 and Y/Z by127,
performs Java f2i/i2b conversions, then combines the sign-extended bytes into
`v`. The X multiplier and sign extension are part of the observed source code.

Two calls outside a batch execute the actual `System.out.println("But..")`
child. A declared null `System.out` call fails before the normal stores.
The same original begin method resets the normal-present flag; a repeated
begin fails while preserving its earlier fields.

Four vertex calls with changing normals execute the original quad expansion.
The fixture prepares only the normal words at duplicate slots3 and4, using
`11223344` and `55667788`. The source duplicate loop copies positions and
optional UV/colors while leaving those normal words untouched. All eight
words of all six emitted vertices are compared. This covers CPU byte emission;
it supplies no GL draw, normal conversion or lighting-pixel evidence.

Capture with the pinned original client archive, SDK jars and JDK17:

```powershell
python -B tests/reference/client/nw_normals/capture_reference.py `
  --client-jar b1.7.3.jar `
  --library-root "$env:APPDATA/.minecraft/libraries" `
  --output .local/nw-normals-original
```

The driver verifies archive, complete selected classes, method Code, tool,
SDK, helper and authored-input hashes. The helper does not replace or transform
game classes. It rejects writes, network/listener attempts and native-library
loads. A successful capture writes30 state/vertex rows to `finite.json` and
retains the bounded process evidence in `capture.json`. Capture and native
replay must complete before these cases receive qualification credit.

Native replay uses the executable and public fixture files, without Java or
the original archive:

```powershell
python -B tests/reference/client/nw_normals/run_native.py `
  --executable build/mc_test_nw_normals.exe --work-dir build/nw-normals
```
