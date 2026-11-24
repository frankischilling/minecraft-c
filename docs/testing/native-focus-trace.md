# Finite Windows Frame/Canvas focus observations

On 2026-11-16, 55 separate ordinary save/title SDK processes completed on physical
monitor 2: 35 with the optional focus observer and 20 without it. Together they
completed 62,870 native checks. The earlier persistent native-Canvas/foreground-
Frame plus absent-SDL-owner failure did not recur at fixture steps in this cohort.
This records a non-reproduction, not a causal explanation or focus fix.

The [sanitized record](../../tests/benchmarks/native_focus_windows.json) contains
the exact source/product identities, per-process capture hashes and finite
results. It was authored after execution. Raw handles, pointers, private paths,
commands, traces, assets and saved World bytes remain private.

| Cohort | Processes | Native checks per process |
| --- | ---: | ---: |
| Traced initial | 5 | 1,127â€“1,163 |
| Traced extended | 30 | 1,109â€“1,166 |
| Untraced control | 20 | 1,097â€“1,184 |

Each cohort preserves 2,388 selected source/helper/product inputs and 413 asset
files before and after execution. The audit re-read all 55 actual native
receipts, copied products and save inventories; replayed the independent
metadata/region decoder; and checked monitor containment, reciprocal host
identities and both windows' reported absence after checked cleanup. These
counts are integration observations, not timings or FPS measurements.

## What the traces observed

The 35 traces contain 10,852 selected-message and fixture records, including 814
successful `active-after` samples. All host samples, including before-step
samples, have non-NULL SDL owners. There are no completed-step mismatch
candidates, active-after NULL/false samples, overwritten records, reentrant
drops or unmatched selected deliveries. Both hooks uninstall successfully.
Complete native exit 0 supplies the additional file-close witness; the JSON
footer is written before close and cannot prove that close by itself.

There **are 70 hook-boundary NULL observations** matching each process's bound
native Canvas and foreground Frame: one `wndproc-after` `WM_KILLFOCUS` (8) and
one `wndproc-before` `WM_SETFOCUS` (7) per trace. These handoff observations are
distinct from a persistent mismatch. The observer changes timing; none of these
results identifies the earlier failure's cause.

Across the successful traces, 420 pre/post pairs have differing creation-message
payload pointers. The reader pairs deliveries by nesting depth, HWND identity
and message, while preserving the raw parameter differences. An initial private
reader instead required payload equality and rejected one attempt after its
native process passed 1,166 checks. That retained attempt is excluded from the
55-process successful cohort. Its reader hash, the successful traced controller
hash and the later untraced controller hash are recorded separately; no later
helper is described as having executed an earlier run.

Two separate startup refusal controls return native exit 1: exclusive creation
preserves an existing trace marker, and an `SDL3_DYNAMIC_API` override is rejected
before dispatch priming or hook installation. Broader failure and abrupt-exit
paths are not qualified by these controls.

## Source and link scope

The native fixture SHA256 is
`ef028354bdc1d1a6e2d7de777b8675b7fc4abb0386ffa269b1fcaef5dc47c861`;
the observer include is
`87d8cf25126ef36456327552c88c65edcbd4bf1eddd63d13d8dcfe8d332d5423`.
The record also identifies the exact runtime/control sources, both successful
private controller versions and the excluded earlier reader version.

The executed binary SHA256 is
`cd0873caca2486300ab2ec3073df964865e0324c2bc8ac20fe97ce568bdf8fda`.
A captured successful Release compile/archive/link and 40 retained provenance
checks connect the corrected generated SDL Windows TU, keyboard implementation,
dynapi stub, exact object/member bytes, static library, linker map and executable.
Normal Release flags remain; map and archive-search diagnostics are additive.
The selection evidence is the map plus exact archive search, not verbose
`Loaded` lines. Exact artifact hashes are in the record. Capture-time pin
equality and retained copies are the historical evidence; volatile build logs
need not remain identical forever.

The configured TU is the generated copy selected by the existing CMake SDL
correction, not the pristine cached source. Artifact linkage does not establish
entered instructions. The older failure's executable hash, `31290d48â€¦`, lacks
matching execution-time source/SDL link proof and is not relabelled as a result
from this binary. [Frame/Canvas scope](../client/frame-canvas.md) remains separate.

## Repeating the finite scenario

Use an already built Windows `mc_test_world_selection_loop` and owned imported
assets. Run serially on physical monitor 2. Require `SDL3_DYNAMIC_API`,
`SDL_DYNAPI_LOG_CALLS` and an inherited `MC_WORLD_SELECTION_FOCUS_TRACE` to be
absent; the observer refuses overrides instead of silently clearing them.
Choose a new private artifact directory and trace filename:

```powershell
$focusArtifacts = Join-Path (Get-Location) ".local/native-focus"
$env:MC_WORLD_SELECTION_FOCUS_TRACE = Join-Path $focusArtifacts "focus.jsonl"
try {
    python -B tests/tools/client/test_world_selection_loop.py --executable build/Release/mc_test_world_selection_loop.exe --assets path/to/imported-assets --artifacts $focusArtifacts --mode save-title --monitor-two --timeout 240
} finally {
    Remove-Item Env:\MC_WORLD_SELECTION_FOCUS_TRACE
}
```

The existing runner creates the artifact directory before the native process;
`CREATE_NEW` prevents trace replacement. The test-only observer primes public
SDL dispatch outside the hooks, then installs current-thread pre/post hooks
before bootstrap. It keeps fixed bounded history and samples only reached
focus operations, preserving their order, short-circuit behavior, LastError,
timeout and acceptance gate. The default path installs no observer or priming
read. No new production SDL/frame repair or Java change is part of this work.

Retain fresh source/product/dependency guards with every later capture. A pass
does not qualify original AWT parity, Linux focus behavior, unrestricted play,
all exit paths or a performance improvement. The public hash projection does
not replace its private raw evidence or supply a standalone desktop replay.

## Integrated validation

The complete Windows Release build and all 1,341 configured tests pass in
454.53 seconds. An independent log audit matches every configured name and ID,
complete unique ordinals and Start/completion joins, with no skipped or unrun
cases and all 1,341 preceding names retained. The four existing Fire reader and
packet CTests also pass under WSL in 30.26 seconds. Formatting checks all 2,473
selected C/header/fragment files. The visible SDK fixture is Windows-only; no
Linux focus observation is claimed.

After the current-only scene declarations were refreshed, serial monitor-2
Furnace, Lever and Redstone Torch cycles pass 10,155, 2,005 and 1,212 checks.
Their 2,345, 2,497 and 2,463 selected inputs respectively, plus assets, remain
unchanged during each process. Each completes its ordinary save/reselection
boundary and source-window cleanup. Historical predecessor scene plans remain
unchanged and receive no inherited qualification from those input declarations.
