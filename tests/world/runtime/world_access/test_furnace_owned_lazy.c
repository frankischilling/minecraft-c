/* Reuse the real SourceSaving/Chunk fixture; the imported main is never run. */
int fol_unused_arrow_main(int, char **);
#define MC_ARROW_OWNED_LEAVES_MAIN fol_unused_arrow_main
#include "test_arrow_owned_leaves.c"
#undef MC_ARROW_OWNED_LEAVES_MAIN

#include "furnace_lazy_controls.inc"

int main(int argc, char **argv) {
    return fol_main(argc, argv);
}
