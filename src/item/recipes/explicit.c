#include "../../block/registry/registry_internal.h"
#include "../registry/stacks_internal.h"
#include "recipes_internal.h"

/* hk/ey.<init>, after the seven family calls. The source's transient Object[]
 * and cached Character operands are typed automatic spans here; the actual
 * Stack and registered recipe owners use the shared retained allocation ledger.
 * Each live static load remains after its reached Stack allocation. */
static mc_registry_status item_stack(mc_crafting_manager *manager, int32_t new_pc, int32_t field_pc,
                                     const char *field, int32_t count, int32_t damage,
                                     int32_t constructor_pc, mc_item_stack **out) {
    mc_registry_classes *classes = manager->classes;
    mc_item_stack *raw = NULL;
    mc_registry_crafting_stage(manager, new_pc);
    mc_registry_status status = mc_registry_stack_allocate(classes, &raw);
    if (status != MC_REGISTRY_OK)
        return mc_registry_crafting_error(manager);
    mc_registry_crafting_stage(manager, field_pc);
    mc_item *item = NULL;
    status = mc_registry_item_static_load(classes, field, &item);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_crafting_stage(manager, constructor_pc);
    status = mc_registry_stack_construct_item_values(classes, raw, item, count, damage);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = raw;
    return MC_REGISTRY_OK;
}

static mc_registry_status block_stack(mc_crafting_manager *manager, int32_t new_pc,
                                      int32_t field_pc, const char *field, int32_t count,
                                      int32_t damage, int32_t constructor_pc, mc_item_stack **out) {
    mc_registry_classes *classes = manager->classes;
    mc_item_stack *raw = NULL;
    mc_registry_crafting_stage(manager, new_pc);
    mc_registry_status status = mc_registry_stack_allocate(classes, &raw);
    if (status != MC_REGISTRY_OK)
        return mc_registry_crafting_error(manager);
    mc_registry_crafting_stage(manager, field_pc);
    mc_block *block = NULL;
    status = mc_registry_block_static_load(classes, field, &block);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_crafting_stage(manager, constructor_pc);
    status = mc_registry_stack_construct_block_values(classes, raw, block, count, damage);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = raw;
    return MC_REGISTRY_OK;
}

static mc_registry_status row(mc_crafting_manager *manager, mc_recipe_argument *argument,
                              int32_t pc, const char *text) {
    mc_registry_crafting_stage(manager, pc);
    const mc_registry_string *literal = NULL;
    const mc_registry_status status = mc_registry_literal_ascii(manager->classes, text, &literal);
    if (status != MC_REGISTRY_OK)
        return mc_registry_crafting_error(manager);
    *argument = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STRING, .value.string = literal};
    return MC_REGISTRY_OK;
}

static mc_registry_status item_argument(mc_crafting_manager *manager, mc_recipe_argument *argument,
                                        int32_t pc, const char *field) {
    mc_registry_crafting_stage(manager, pc);
    mc_item *item = NULL;
    const mc_registry_status status = mc_registry_item_static_load(manager->classes, field, &item);
    if (status != MC_REGISTRY_OK)
        return status;
    *argument = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_ITEM, .value.item = item};
    return MC_REGISTRY_OK;
}

static mc_registry_status block_argument(mc_crafting_manager *manager, mc_recipe_argument *argument,
                                         int32_t pc, const char *field) {
    mc_registry_crafting_stage(manager, pc);
    mc_block *block = NULL;
    const mc_registry_status status =
        mc_registry_block_static_load(manager->classes, field, &block);
    if (status != MC_REGISTRY_OK)
        return status;
    *argument = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_BLOCK, .value.block = block};
    return MC_REGISTRY_OK;
}

#define CALL(expression)                                                                           \
    do {                                                                                           \
        const mc_registry_status call_status = (expression);                                       \
        if (call_status != MC_REGISTRY_OK)                                                         \
            return call_status;                                                                    \
    } while (0)
#define OUTPUT_ITEM(new_pc, field_pc, field, count, damage, constructor_pc)                        \
    CALL(item_stack(manager, new_pc, field_pc, field, count, damage, constructor_pc, &output))
#define OUTPUT_BLOCK(new_pc, field_pc, field, count, damage, constructor_pc)                       \
    CALL(block_stack(manager, new_pc, field_pc, field, count, damage, constructor_pc, &output))
#define ARGUMENTS(pc, count)                                                                       \
    do {                                                                                           \
        mc_registry_crafting_stage(manager, pc);                                                   \
        argument_count = (count);                                                                  \
        for (size_t argument_index = 0; argument_index < argument_count; ++argument_index)         \
            arguments[argument_index] = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_NULL};     \
    } while (0)
#define ROW(index, pc, text) CALL(row(manager, &arguments[index], pc, text))
#define CHARACTER(index, pc, character_value)                                                      \
    do {                                                                                           \
        mc_registry_crafting_stage(manager, pc);                                                   \
        arguments[index] = (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_CHARACTER,              \
                                                .value.character = (character_value)};             \
    } while (0)
#define ITEM(index, pc, field) CALL(item_argument(manager, &arguments[index], pc, field))
#define BLOCK(index, pc, field) CALL(block_argument(manager, &arguments[index], pc, field))
#define SHAPED(pc)                                                                                 \
    do {                                                                                           \
        mc_registry_crafting_stage(manager, pc);                                                   \
        CALL(mc_registry_recipe_shaped(manager, output, arguments, argument_count));               \
    } while (0)

mc_registry_status mc_registry_recipe_explicit(mc_registry_classes *classes,
                                               mc_crafting_manager *manager) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (manager == NULL || manager->classes != classes || classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "explicit recipes require their admitted manager owner", NULL);

    mc_item_stack *output = NULL;
    mc_recipe_argument arguments[11];
    size_t argument_count = 0;

    /* Source PCs 92..127. */
    OUTPUT_ITEM(93, 97, "aI", 3, 0, 101);
    ARGUMENTS(105, 3);
    ROW(0, 110, "###");
    CHARACTER(1, 117, 35);
    ITEM(2, 123, "aH");
    SHAPED(127);

    OUTPUT_ITEM(131, 135, "aJ", 1, 0, 139);
    ARGUMENTS(143, 5);
    ROW(0, 148, "#");
    ROW(1, 153, "#");
    ROW(2, 158, "#");
    CHARACTER(3, 165, 35);
    ITEM(4, 171, "aI");
    SHAPED(175);

    OUTPUT_BLOCK(179, 183, "ba", 2, 0, 187);
    ARGUMENTS(191, 4);
    ROW(0, 196, "###");
    ROW(1, 201, "###");
    CHARACTER(2, 208, 35);
    ITEM(3, 214, "B");
    SHAPED(218);

    OUTPUT_BLOCK(222, 226, "aZ", 1, 0, 230);
    ARGUMENTS(235, 7);
    ROW(0, 240, "###");
    ROW(1, 245, "#X#");
    ROW(2, 250, "###");
    CHARACTER(3, 257, 35);
    BLOCK(4, 263, "y");
    CHARACTER(5, 271, 88);
    ITEM(6, 278, "l");
    SHAPED(282);

    OUTPUT_BLOCK(286, 290, "S", 1, 0, 294);
    ARGUMENTS(299, 7);
    ROW(0, 304, "###");
    ROW(1, 309, "#X#");
    ROW(2, 314, "###");
    CHARACTER(3, 321, 35);
    BLOCK(4, 327, "y");
    CHARACTER(5, 335, 88);
    ITEM(6, 342, "aA");
    SHAPED(346);

    OUTPUT_BLOCK(350, 354, "ao", 1, 0, 358);
    ARGUMENTS(363, 7);
    ROW(0, 368, "###");
    ROW(1, 373, "XXX");
    ROW(2, 378, "###");
    CHARACTER(3, 385, 35);
    BLOCK(4, 391, "y");
    CHARACTER(5, 399, 88);
    ITEM(6, 406, "aJ");
    SHAPED(410);

    OUTPUT_BLOCK(414, 418, "aV", 1, 0, 422);
    ARGUMENTS(426, 4);
    ROW(0, 431, "##");
    ROW(1, 436, "##");
    CHARACTER(2, 443, 35);
    ITEM(3, 449, "aB");
    SHAPED(453);

    OUTPUT_BLOCK(457, 461, "aX", 1, 0, 465);
    ARGUMENTS(469, 4);
    ROW(0, 474, "##");
    ROW(1, 479, "##");
    CHARACTER(2, 486, 35);
    ITEM(3, 492, "aG");
    SHAPED(496);

    OUTPUT_BLOCK(500, 504, "am", 1, 0, 508);
    ARGUMENTS(512, 4);
    ROW(0, 517, "##");
    ROW(1, 522, "##");
    CHARACTER(2, 529, 35);
    ITEM(3, 535, "aF");
    SHAPED(539);

    OUTPUT_BLOCK(543, 547, "be", 1, 0, 551);
    ARGUMENTS(555, 4);
    ROW(0, 560, "##");
    ROW(1, 565, "##");
    CHARACTER(2, 572, 35);
    ITEM(3, 578, "aR");
    SHAPED(582);

    OUTPUT_BLOCK(586, 590, "ac", 1, 0, 594);
    ARGUMENTS(598, 4);
    ROW(0, 603, "##");
    ROW(1, 608, "##");
    CHARACTER(2, 615, 35);
    ITEM(3, 621, "I");
    SHAPED(625);

    OUTPUT_BLOCK(629, 633, "an", 1, 0, 637);
    ARGUMENTS(642, 7);
    ROW(0, 647, "X#X");
    ROW(1, 652, "#X#");
    ROW(2, 657, "X#X");
    CHARACTER(3, 664, 88);
    ITEM(4, 670, "K");
    CHARACTER(5, 678, 35);
    BLOCK(6, 685, "F");
    SHAPED(689);

    /* Four distinct metadata-bearing Block/count/damage constructions. */
    OUTPUT_BLOCK(693, 697, "al", 3, 3, 702);
    ARGUMENTS(706, 3);
    ROW(0, 711, "###");
    CHARACTER(1, 718, 35);
    BLOCK(2, 724, "x");
    SHAPED(728);

    OUTPUT_BLOCK(732, 736, "al", 3, 0, 741);
    ARGUMENTS(745, 3);
    ROW(0, 750, "###");
    CHARACTER(1, 757, 35);
    BLOCK(2, 763, "u");
    SHAPED(767);

    OUTPUT_BLOCK(771, 775, "al", 3, 1, 780);
    ARGUMENTS(784, 3);
    ROW(0, 789, "###");
    CHARACTER(1, 796, 35);
    BLOCK(2, 802, "R");
    SHAPED(806);

    OUTPUT_BLOCK(810, 814, "al", 3, 2, 819);
    ARGUMENTS(823, 3);
    ROW(0, 828, "###");
    CHARACTER(1, 835, 35);
    BLOCK(2, 841, "y");
    SHAPED(845);

    OUTPUT_BLOCK(849, 853, "aG", 2, 0, 857);
    ARGUMENTS(861, 5);
    ROW(0, 866, "# #");
    ROW(1, 871, "###");
    ROW(2, 876, "# #");
    CHARACTER(3, 883, 35);
    ITEM(4, 889, "B");
    SHAPED(893);

    OUTPUT_ITEM(897, 901, "at", 1, 0, 905);
    ARGUMENTS(909, 5);
    ROW(0, 914, "##");
    ROW(1, 919, "##");
    ROW(2, 924, "##");
    CHARACTER(3, 931, 35);
    BLOCK(4, 937, "y");
    SHAPED(941);

    OUTPUT_BLOCK(945, 949, "bl", 2, 0, 953);
    ARGUMENTS(957, 4);
    ROW(0, 962, "###");
    ROW(1, 967, "###");
    CHARACTER(2, 974, 35);
    BLOCK(3, 980, "y");
    SHAPED(984);

    OUTPUT_ITEM(988, 992, "az", 1, 0, 996);
    ARGUMENTS(1000, 5);
    ROW(0, 1005, "##");
    ROW(1, 1010, "##");
    ROW(2, 1015, "##");
    CHARACTER(3, 1022, 35);
    ITEM(4, 1028, "m");
    SHAPED(1032);

    OUTPUT_ITEM(1036, 1040, "as", 1, 0, 1044);
    ARGUMENTS(1049, 7);
    ROW(0, 1054, "###");
    ROW(1, 1059, "###");
    ROW(2, 1064, " X ");
    CHARACTER(3, 1071, 35);
    BLOCK(4, 1077, "y");
    CHARACTER(5, 1085, 88);
    ITEM(6, 1092, "B");
    SHAPED(1096);

    OUTPUT_ITEM(1100, 1104, "aX", 1, 0, 1108);
    ARGUMENTS(1113, 11);
    ROW(0, 1118, "AAA");
    ROW(1, 1123, "BEB");
    ROW(2, 1128, "CCC");
    CHARACTER(3, 1135, 65);
    ITEM(4, 1141, "aE");
    CHARACTER(5, 1149, 66);
    ITEM(6, 1156, "aW");
    CHARACTER(7, 1165, 67);
    ITEM(8, 1172, "R");
    CHARACTER(9, 1181, 69);
    ITEM(10, 1188, "aN");
    SHAPED(1192);

    OUTPUT_ITEM(1196, 1200, "aW", 1, 0, 1204);
    ARGUMENTS(1208, 3);
    ROW(0, 1213, "#");
    CHARACTER(1, 1220, 35);
    ITEM(2, 1226, "aH");
    SHAPED(1230);

    OUTPUT_BLOCK(1234, 1238, "y", 4, 0, 1242);
    ARGUMENTS(1246, 3);
    ROW(0, 1251, "#");
    CHARACTER(1, 1258, 35);
    BLOCK(2, 1264, "K");
    SHAPED(1268);

    OUTPUT_ITEM(1272, 1276, "B", 4, 0, 1280);
    ARGUMENTS(1284, 4);
    ROW(0, 1289, "#");
    ROW(1, 1294, "#");
    CHARACTER(2, 1301, 35);
    BLOCK(3, 1307, "y");
    SHAPED(1311);

    OUTPUT_BLOCK(1315, 1319, "ar", 4, 0, 1323);
    ARGUMENTS(1328, 6);
    ROW(0, 1333, "X");
    ROW(1, 1338, "#");
    CHARACTER(2, 1345, 88);
    ITEM(3, 1351, "k");
    CHARACTER(4, 1359, 35);
    ITEM(5, 1365, "B");
    SHAPED(1369);

    OUTPUT_BLOCK(1373, 1377, "ar", 4, 0, 1381);
    ARGUMENTS(1386, 6);
    ROW(0, 1391, "X");
    ROW(1, 1396, "#");
    CHARACTER(2, 1403, 88);
    mc_item_stack *ingredient = NULL;
    CALL(item_stack(manager, 1409, 1413, "k", 1, 1, 1418, &ingredient));
    arguments[3] =
        (mc_recipe_argument){.kind = MC_RECIPE_ARGUMENT_STACK, .value.stack = ingredient};
    CHARACTER(4, 1426, 35);
    ITEM(5, 1432, "B");
    SHAPED(1436);

    OUTPUT_ITEM(1440, 1444, "C", 4, 0, 1448);
    ARGUMENTS(1452, 4);
    ROW(0, 1457, "# #");
    ROW(1, 1462, " # ");
    CHARACTER(2, 1469, 35);
    BLOCK(3, 1475, "y");
    SHAPED(1479);

    OUTPUT_BLOCK(1483, 1487, "aH", 16, 0, 1492);
    ARGUMENTS(1497, 7);
    ROW(0, 1502, "X X");
    ROW(1, 1507, "X#X");
    ROW(2, 1512, "X X");
    CHARACTER(3, 1519, 88);
    ITEM(4, 1525, "m");
    CHARACTER(5, 1533, 35);
    ITEM(6, 1540, "B");
    SHAPED(1544);

    OUTPUT_BLOCK(1548, 1552, "U", 6, 0, 1557);
    ARGUMENTS(1562, 9);
    ROW(0, 1567, "X X");
    ROW(1, 1572, "X#X");
    ROW(2, 1577, "XRX");
    CHARACTER(3, 1584, 88);
    ITEM(4, 1590, "n");
    CHARACTER(5, 1598, 82);
    ITEM(6, 1605, "aA");
    CHARACTER(7, 1614, 35);
    ITEM(8, 1621, "B");
    SHAPED(1625);

    OUTPUT_BLOCK(1629, 1633, "V", 6, 0, 1638);
    ARGUMENTS(1643, 9);
    ROW(0, 1648, "X X");
    ROW(1, 1653, "X#X");
    ROW(2, 1658, "XRX");
    CHARACTER(3, 1665, 88);
    ITEM(4, 1671, "m");
    CHARACTER(5, 1679, 82);
    ITEM(6, 1686, "aA");
    CHARACTER(7, 1695, 35);
    BLOCK(8, 1702, "aL");
    SHAPED(1706);

    OUTPUT_ITEM(1710, 1714, "ax", 1, 0, 1718);
    ARGUMENTS(1722, 4);
    ROW(0, 1727, "# #");
    ROW(1, 1732, "###");
    CHARACTER(2, 1739, 35);
    ITEM(3, 1745, "m");
    SHAPED(1749);

    OUTPUT_BLOCK(1753, 1757, "bg", 1, 0, 1761);
    ARGUMENTS(1766, 6);
    ROW(0, 1771, "A");
    ROW(1, 1776, "B");
    CHARACTER(2, 1783, 65);
    BLOCK(3, 1789, "bb");
    CHARACTER(4, 1797, 66);
    BLOCK(5, 1803, "ar");
    SHAPED(1807);

    OUTPUT_ITEM(1811, 1815, "aL", 1, 0, 1819);
    ARGUMENTS(1824, 6);
    ROW(0, 1829, "A");
    ROW(1, 1834, "B");
    CHARACTER(2, 1841, 65);
    BLOCK(3, 1847, "av");
    CHARACTER(4, 1855, 66);
    ITEM(5, 1861, "ax");
    SHAPED(1865);

    OUTPUT_ITEM(1869, 1873, "aM", 1, 0, 1877);
    ARGUMENTS(1882, 6);
    ROW(0, 1887, "A");
    ROW(1, 1892, "B");
    CHARACTER(2, 1899, 65);
    BLOCK(3, 1905, "aC");
    CHARACTER(4, 1913, 66);
    ITEM(5, 1919, "ax");
    SHAPED(1923);

    OUTPUT_ITEM(1927, 1931, "aC", 1, 0, 1935);
    ARGUMENTS(1939, 4);
    ROW(0, 1944, "# #");
    ROW(1, 1949, "###");
    CHARACTER(2, 1956, 35);
    BLOCK(3, 1962, "y");
    SHAPED(1966);

    OUTPUT_ITEM(1970, 1974, "au", 1, 0, 1978);
    ARGUMENTS(1982, 4);
    ROW(0, 1987, "# #");
    ROW(1, 1992, " # ");
    CHARACTER(2, 1999, 35);
    ITEM(3, 2005, "m");
    SHAPED(2009);

    OUTPUT_ITEM(2013, 2017, "g", 1, 0, 2021);
    ARGUMENTS(2026, 6);
    ROW(0, 2031, "A ");
    ROW(1, 2036, " B");
    CHARACTER(2, 2043, 65);
    ITEM(3, 2049, "m");
    CHARACTER(4, 2057, 66);
    ITEM(5, 2063, "an");
    SHAPED(2067);

    OUTPUT_ITEM(2071, 2075, "S", 1, 0, 2079);
    ARGUMENTS(2083, 3);
    ROW(0, 2088, "###");
    CHARACTER(1, 2095, 35);
    ITEM(2, 2101, "R");
    SHAPED(2105);

    OUTPUT_BLOCK(2109, 2113, "au", 4, 0, 2117);
    ARGUMENTS(2121, 5);
    ROW(0, 2126, "#  ");
    ROW(1, 2131, "## ");
    ROW(2, 2136, "###");
    CHARACTER(3, 2143, 35);
    BLOCK(4, 2149, "y");
    SHAPED(2153);

    OUTPUT_ITEM(2157, 2161, "aP", 1, 0, 2165);
    ARGUMENTS(2170, 7);
    ROW(0, 2175, "  #");
    ROW(1, 2180, " #X");
    ROW(2, 2185, "# X");
    CHARACTER(3, 2192, 35);
    ITEM(4, 2198, "B");
    CHARACTER(5, 2206, 88);
    ITEM(6, 2213, "I");
    SHAPED(2217);

    OUTPUT_BLOCK(2221, 2225, "aI", 4, 0, 2229);
    ARGUMENTS(2233, 5);
    ROW(0, 2238, "#  ");
    ROW(1, 2243, "## ");
    ROW(2, 2248, "###");
    CHARACTER(3, 2255, 35);
    BLOCK(4, 2261, "x");
    SHAPED(2265);

    OUTPUT_ITEM(2269, 2273, "aq", 1, 0, 2277);
    ARGUMENTS(2282, 7);
    ROW(0, 2287, "###");
    ROW(1, 2292, "#X#");
    ROW(2, 2297, "###");
    CHARACTER(3, 2304, 35);
    ITEM(4, 2310, "B");
    CHARACTER(5, 2318, 88);
    BLOCK(6, 2325, "ac");
    SHAPED(2329);

    OUTPUT_ITEM(2333, 2337, "ar", 1, 0, 2341);
    ARGUMENTS(2346, 7);
    ROW(0, 2351, "###");
    ROW(1, 2356, "#X#");
    ROW(2, 2361, "###");
    CHARACTER(3, 2368, 35);
    BLOCK(4, 2374, "ai");
    CHARACTER(5, 2382, 88);
    ITEM(6, 2389, "h");
    SHAPED(2393);

    OUTPUT_BLOCK(2397, 2401, "aK", 1, 0, 2405);
    ARGUMENTS(2410, 6);
    ROW(0, 2415, "X");
    ROW(1, 2420, "#");
    CHARACTER(2, 2427, 35);
    BLOCK(3, 2433, "x");
    CHARACTER(4, 2441, 88);
    ITEM(5, 2447, "B");
    SHAPED(2451);

    OUTPUT_BLOCK(2455, 2459, "aR", 1, 0, 2463);
    ARGUMENTS(2468, 6);
    ROW(0, 2473, "X");
    ROW(1, 2478, "#");
    CHARACTER(2, 2485, 35);
    ITEM(3, 2491, "B");
    CHARACTER(4, 2499, 88);
    ITEM(5, 2505, "aA");
    SHAPED(2509);

    OUTPUT_ITEM(2513, 2517, "aZ", 1, 0, 2521);
    ARGUMENTS(2526, 8);
    ROW(0, 2531, "#X#");
    ROW(1, 2536, "III");
    CHARACTER(2, 2543, 35);
    BLOCK(3, 2549, "aR");
    CHARACTER(4, 2557, 88);
    ITEM(5, 2563, "aA");
    CHARACTER(6, 2572, 73);
    BLOCK(7, 2579, "u");
    SHAPED(2583);

    OUTPUT_ITEM(2587, 2591, "aQ", 1, 0, 2595);
    ARGUMENTS(2600, 7);
    ROW(0, 2605, " # ");
    ROW(1, 2610, "#X#");
    ROW(2, 2615, " # ");
    CHARACTER(3, 2622, 35);
    ITEM(4, 2628, "n");
    CHARACTER(5, 2636, 88);
    ITEM(6, 2643, "aA");
    SHAPED(2647);

    OUTPUT_ITEM(2651, 2655, "aO", 1, 0, 2659);
    ARGUMENTS(2664, 7);
    ROW(0, 2669, " # ");
    ROW(1, 2674, "#X#");
    ROW(2, 2679, " # ");
    CHARACTER(3, 2686, 35);
    ITEM(4, 2692, "m");
    CHARACTER(5, 2700, 88);
    ITEM(6, 2707, "aA");
    SHAPED(2711);

    OUTPUT_ITEM(2715, 2719, "bb", 1, 0, 2723);
    ARGUMENTS(2728, 7);
    ROW(0, 2733, "###");
    ROW(1, 2738, "#X#");
    ROW(2, 2743, "###");
    CHARACTER(3, 2750, 35);
    ITEM(4, 2756, "aI");
    CHARACTER(5, 2764, 88);
    ITEM(6, 2771, "aO");
    SHAPED(2775);

    OUTPUT_BLOCK(2779, 2783, "aS", 1, 0, 2787);
    ARGUMENTS(2791, 4);
    ROW(0, 2796, "#");
    ROW(1, 2801, "#");
    CHARACTER(2, 2808, 35);
    BLOCK(3, 2814, "u");
    SHAPED(2818);

    OUTPUT_BLOCK(2822, 2826, "aL", 1, 0, 2830);
    ARGUMENTS(2834, 3);
    ROW(0, 2839, "##");
    CHARACTER(1, 2846, 35);
    BLOCK(2, 2852, "u");
    SHAPED(2856);

    OUTPUT_BLOCK(2860, 2864, "aN", 1, 0, 2868);
    ARGUMENTS(2872, 3);
    ROW(0, 2877, "##");
    CHARACTER(1, 2884, 35);
    BLOCK(2, 2890, "y");
    SHAPED(2894);

    OUTPUT_BLOCK(2898, 2902, "Q", 1, 0, 2906);
    ARGUMENTS(2911, 9);
    ROW(0, 2916, "###");
    ROW(1, 2921, "#X#");
    ROW(2, 2926, "#R#");
    CHARACTER(3, 2933, 35);
    BLOCK(4, 2939, "x");
    CHARACTER(5, 2947, 88);
    ITEM(6, 2954, "i");
    CHARACTER(7, 2963, 82);
    ITEM(8, 2970, "aA");
    SHAPED(2974);

    OUTPUT_BLOCK(2978, 2982, "aa", 1, 0, 2986);
    ARGUMENTS(2991, 11);
    ROW(0, 2996, "TTT");
    ROW(1, 3001, "#X#");
    ROW(2, 3006, "#R#");
    CHARACTER(3, 3013, 35);
    BLOCK(4, 3019, "x");
    CHARACTER(5, 3027, 88);
    ITEM(6, 3034, "m");
    CHARACTER(7, 3043, 82);
    ITEM(8, 3050, "aA");
    CHARACTER(9, 3059, 84);
    BLOCK(10, 3066, "y");
    SHAPED(3070);

    OUTPUT_BLOCK(3074, 3078, "W", 1, 0, 3082);
    ARGUMENTS(3087, 6);
    ROW(0, 3092, "S");
    ROW(1, 3097, "P");
    CHARACTER(2, 3104, 83);
    ITEM(3, 3110, "aK");
    CHARACTER(4, 3118, 80);
    BLOCK(5, 3124, "aa");
    SHAPED(3128);

    OUTPUT_ITEM(3132, 3136, "aY", 1, 0, 3140);
    ARGUMENTS(3145, 6);
    ROW(0, 3150, "###");
    ROW(1, 3155, "XXX");
    CHARACTER(2, 3162, 35);
    BLOCK(3, 3168, "ac");
    CHARACTER(4, 3176, 88);
    BLOCK(5, 3182, "y");
    SHAPED(3186);
    return MC_REGISTRY_OK;
}

#undef SHAPED
#undef BLOCK
#undef ITEM
#undef CHARACTER
#undef ROW
#undef ARGUMENTS
#undef OUTPUT_BLOCK
#undef OUTPUT_ITEM
#undef CALL
