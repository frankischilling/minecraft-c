# Local reference launch observations

Observed 2026-10-17 on Windows 10 Home 10.0.19045, x86-64, Intel Core i9-10900F.
Binary fingerprints are in [the audit](binaries.md).

## Dedicated server

The untouched supplied server ran using Eclipse Temurin `17.0.17+10`:

```powershell
python scripts/reference/server/run_smoke.py --jar b1.7.3_server.jar --work-dir .local/server-smoke-01
```

The helper verifies the known hash and refuses an existing work directory. It
writes normal server properties with `server-ip=127.0.0.1`, `server-port=25575`,
`online-mode=false`, seed `17`, `allow-nether=false`, no animal/monster spawning,
and a fresh `smoke-world`. It invokes the original main class with `nogui` and
`-Djava.awt.headless=true -Xms128M -Xmx768M`. No firewall/router setting changed.

Observed output reported `Starting minecraft server version Beta 1.7.3`, reached
`Done`, accepted `list`, `save-all`, and `stop`, reported save completion and
`Saving chunks`, and exited 0. `smoke-world/level.dat` exists. Startup also printed
151 recipes and 16 achievements. Missing ban/operator/whitelist files generated
the reference's initial warnings. Offline identity warnings were expected.

Local output, configuration and generated worlds remain in the ignored work
directory. This is a server startup/save/stop observation. It does not establish
any successful client join, protocol exchange, save interoperability, recipe
correctness, or C runtime behavior. Startup elapsed time is not a benchmark.

Use a fresh work-directory name to repeat. Keep the previous directory as an
untouched baseline; copy it into another ignored directory before any later test
that can write it. Restore a working copy by replacing that copy from the untouched
baseline. No personal world was opened or modified by this smoke test.

## Ordinary client launcher

The computer-use app inventory returned exactly one Minecraft Launcher window.
Two attempts to capture that returned window, with refreshed selection between
attempts, failed with `FrameArrived timed out: timed out waiting on channel` and
`window capture timed out: timed out waiting on channel`.

No Play action was attempted without an observable window. The ordinary graphical
launch workflow is currently unavailable through this capture tool. The signed-in
account was not used as a credential source; no account files, session tokens,
or launcher logs were read. Installed `versions/b1.7.3` contains the client JAR,
version metadata, and server JAR, and the resource directories exist, but files
on disk do not prove a successful graphical launch.

Retry the ordinary launcher workflow when UI capture works. Original client to
original server remains the first multiplayer runtime baseline. An alternate
instrumented launch must be tracked separately from ordinary-launch evidence.
