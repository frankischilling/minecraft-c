# Native launch product capture

Visible World test controllers now preserve native products before starting
the selected executable. A later build can replace the original file without
removing the captured image. This addresses a gap found while investigating
the retained Torch focus failures: those records contained an executable hash,
but the failed executable's bytes were no longer available for inspection.

## Captured scope

Each new evidence directory contains `native-products/manifest.json`, the
executable, adjacent DLLs or versioned shared libraries, and available SDL
build inputs. Before/copy/preserved/after hashes and sizes must agree, and
source file identity must remain stable during capture. Copies never replace
source products or an earlier evidence directory. Original archives, imported
textures and saved user data are outside this product selection.

SDL archives and generated native source copies are labelled **candidates**.
Their presence does not prove that an executable linked those bytes. A later
binary/member comparison must establish that connection. Missing optional
roles are recorded explicitly. Adjacent-library capture likewise does not
inventory system libraries or prove the process's actual DLL search result.

The existing command still launches its selected original executable path;
capture does not redirect execution to the saved copy. Controllers retain
their ordinary input-continuity checks. A concurrent external replacement
between capture and loading is not excluded by the copy alone.

## Failures and ownership

A failed product capture writes a partial receipt where possible and prevents
native launch. Existing product evidence remains untouched; collision reports
use a separate manifest. Partial copied bytes remain available for diagnosis.

Once the process starts, its result or launch error stays primary. A failed
final save snapshot cannot erase the executable, stdout, stderr or original
failure. A successful process still reports a failed final snapshot. These
rules apply to failure records as well as successful gameplay captures.

No game timing, focus predicate, input, work budget, World state or Random
stream changes. Capture runs before process launch. The ordinary client and
its distributed runtime are unaffected.

## Verification and remaining focus work

Eighteen filesystem and controller tests pass on Windows and Linux. They cover
later source replacement, partial copies, source changes during capture,
existing evidence, omitted non-native files, launch refusal and preservation
of primary process failures when the final snapshot also fails.

The first two-mode World validation of this capture passes placement and
fails support removal during held-button mining. Placement, geometry and
lighting finish before that failure. Windows reports the Canvas as focused
under the foreground Frame, while SDL reports no keyboard window. The failed
executable and all four selected products are preserved; their capture hashes
agree. A separate comparison binds the corrected `WIN_UpdateFocus` body in
that failed executable to the retained SDL archive, including its non-relocated
bytes and relevant imports. The existing correction was present in this fresh
failure; its event-order cause remains unresolved.

Private controls pass eight basic Frame/Canvas lifecycles, eight with 128
passive event pumps each, eight with a real GL context and the same passive
observations, eight with repeated native focus requests, eight adding relative
mouse mode, 56 SDK reactivations, and six earlier fresh World scenarios. All
outcomes remain retained. These finite controls do not establish general focus
reliability or bind the missing historical executable. No new focus fix is
claimed.

Run the asset-independent checks with:

```text
ctest --test-dir build-windows -R "^tools.native_product_capture$" --output-on-failure
```
