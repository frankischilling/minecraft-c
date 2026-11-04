# BlockItem placement metadata

BlockItem placement now dispatches the original Item metadata method for Wool,
Slabs, Logs, Saplings, Leaves and both Pistons. These six overrides previously
stopped before the World setter. The ordinary Stack damage read still happens
before dispatch, including when the selected method ignores its argument.

| Item family | Result before the Chunk stores its metadata nibble |
| --- | --- |
| Wool, Slab, Log, Sapling | The complete signed damage value |
| Leaves | Damage bitwise OR 8, without masking other bits |
| Piston and Sticky Piston | 7 |
| Every other registered Item | Inherited base result 0 |

The shared internal dispatch reads no fields, initializes no classes and
allocates nothing. It accepts only the 38 independently mapped Item kinds.
Unknown kinds fail native admission without changing the output.

## Verification

The authority is the supplied original client archive with SHA256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The [original scalar packet](../../tests/reference/item/placement_metadata/README.md)
records the actual registered receivers and their declaring methods.

Two fresh original VMs produce identical observations for all 202 registered
Items, all 38 receiver classes and 20 signed operands per Item. Windows MSVC
Release and Linux Clang Debug with ASan/UBSan match all 4,040 scalar results
and the complete class inventory. Seven native comparator controls reject
changed transcripts; the original packet adds 37 artifact/row controls.

The Item-use component passes 861 checks on both hosts. Five new calls enter the
real BlockItem path and write Wool, Log or Leaves metadata through the normal
notifying setter. They complete placed hooks, sound and the Stack count store,
then reach the declared NULL Player statistic failure. This includes negative
Wool damage and Leaves with the lowest signed damage value. The Chunk stores
the low four bits; the method receipt retains the complete signed result.

Separate controls keep count-zero return ahead of metadata dispatch. Five
families reach a normal false setter at height 128, preserving the Stack count
and skipping sound and placed hooks before the later statistic invocation.
Sapling still stops at its earlier unimplemented placement predicate.

Eight focused tests pass on each host, including controller use, Food, farming,
Dye, both metadata tests and the sealed historical Workbench receipt. The
Linux cohort completes in 7.52 CTest seconds with leak detection and
halt-on-error. The new code has no fresh Workbench SDK qualification.

The first native replay mislabeled three Buckets because its observer read the
last constructor/mutator diagnostic as a runtime Class. The corrected fixture
maps the actual Item kind. All metadata values already matched in that failed
run; its complete output remains retained.

These finite method and component checks do not qualify all six families for
ordinary loaded play. Other Block predicates, hooks, Item drawing, original
pixels/audio, save/reload and frame rate retain their separate scopes.
