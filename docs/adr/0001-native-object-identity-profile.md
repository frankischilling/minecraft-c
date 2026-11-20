---
status: accepted
date: 2026-11-16
---

# Native startup for object identity

Use an application-owned native identity stream that outlives Worlds, with the selected HotSpot lazy hashing algorithm and Java HashSet behavior. This approved runtime choice replaces selected-JVM startup-history matching; cached object hashes, current Thread consumption, collisions, table/tree history and iteration order remain required. Observed original hashes, Thread words and traversal orders are evidence outputs and must not become native execution inputs.
