# Source SaveHandler

The source SaveHandler uses the same opaque WorldStore identity as the storage
module. Its native class scope retains the shared File namespace and Logger.
The constructor follows the original order: initialize the Logger, allocate the
Store, sample the session token, construct the world/players/data Files, create
the reached directories, and write the eight-byte big-endian `session.lock`.
SaveFormat returns that same completed Store. Failed candidates retain their
actual Files, streams, directory effects and typed causes for cleanup.

Metadata reads use the real FileInputStream, GZIP, DataInput/NBT and WorldInfo
owners. The method checks `level.dat`, then `level.dat_old` when the primary is
absent. If both are absent it returns NULL. A caught read/schema exception stops
at its actual, unfinished `printStackTrace` call; it does not silently continue
to the fallback. Successful reads transfer the actual WorldInfo occurrence.
Retained document/Data/Info aliases remain valid until their own holds end.

Session checking reads exactly eight bytes through the existing DataInput
scratch/readFully implementation, compares the live Store token, closes the
stream and returns on a match. Short reads retain their actual intake and
finally-close effects. A mismatch or caught I/O error reaches the original
exception allocation, which remains an explicit dependency. Native cleanup
never deletes the lock, invents a source exception, or repeats a consumed close.

The constructor cohort passes twenty selected tests per compiler. The later
metadata/session cohort passes a 27-case scope on Windows, Clang and GCC;
Windows retains its original stale-test failure and the corrected affected
rerun. Linux uses ASan, UBSan and leak detection. New ordinary and close-fault
read controls pass 257 and 283 checks per compiler. These are authored native
checks, without an original-game execution or a complete World session.

Source-owned Stores reject legacy whole-file operations before those paths
perform effects. Provider/region-loader dispatch, the remaining source write
methods, exception/printing support and connected World loading remain
unfinished. Existing legacy storage compatibility results retain their separate
scope; this work does not establish source-backed save/reload or gameplay.
