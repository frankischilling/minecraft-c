#include "registry_internal.h"

/* Source-order uu/na.<clinit> registration instructions, PCs 192..3395.
 * The individual constructor/mutator helpers own their reached effects.
 * No final-property table substitutes for the ordered calls below. */

const char *mc_registry_block_static_name(size_t index) {
    static const char *const names[] = {
        "u",  "v",  "w",  "x",  "y",  "z",  "A",  "B",  "C",  "D",  "E",  "F",  "G",  "H",
        "I",  "J",  "K",  "L",  "M",  "N",  "O",  "P",  "Q",  "R",  "S",  "T",  "U",  "V",
        "W",  "X",  "Y",  "Z",  "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",
        "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at", "au", "av", "aw", "ax",
        "ay", "az", "aA", "aB", "aC", "aD", "aE", "aF", "aG", "aH", "aI", "aJ", "aK", "aL",
        "aM", "aN", "aO", "aP", "aQ", "aR", "aS", "aT", "aU", "aV", "aW", "aX", "aY", "aZ",
        "ba", "bb", "bc", "bd", "be", "bf", "bg", "bh", "bi", "bj", "bk", "bl"};
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : NULL;
}

/* These statement macros preserve explicit source sites without introducing
 * an interpreter or moving argument reads before RAW allocation. */
#define AT(pc) (classes->block_initializer_pc = (pc))
#define TRY(pc, expression)                                                                        \
    do {                                                                                           \
        AT(pc);                                                                                    \
        status = (expression);                                                                     \
        if (status != MC_REGISTRY_OK) {                                                            \
            return status;                                                                         \
        }                                                                                          \
    } while (0)
#define NEW(pc, kind)                                                                              \
    do {                                                                                           \
        TRY(pc, mc_registry_block_allocate(classes, (kind), &block));                              \
        arguments = (mc_block_arguments){0};                                                       \
    } while (0)
#define ARG(pc, field, value)                                                                      \
    do {                                                                                           \
        AT(pc);                                                                                    \
        arguments.field = (value);                                                                 \
    } while (0)
#define CTOR(pc) TRY(pc, mc_registry_block_construct(classes, block, &arguments))
#define H(pc, value)                                                                               \
    do {                                                                                           \
        AT(pc);                                                                                    \
        mc_registry_block_hardness(block, (value));                                                \
    } while (0)
#define R(pc, value)                                                                               \
    do {                                                                                           \
        AT(pc);                                                                                    \
        mc_registry_block_resistance(block, (value));                                              \
    } while (0)
#define E(pc, value) TRY(pc, mc_registry_block_emission(classes, block, (value)))
#define O(pc, value) TRY(pc, mc_registry_block_opacity(classes, block, (value)))
#define SOUND(load_pc, field, call_pc)                                                             \
    do {                                                                                           \
        AT(load_pc);                                                                               \
        sound = classes->blocks.field;                                                             \
        AT(call_pc);                                                                               \
        mc_registry_block_sound(block, sound);                                                     \
    } while (0)
#define NAME(load_pc, text, call_pc)                                                               \
    do {                                                                                           \
        AT(load_pc);                                                                               \
        suffix = (text);                                                                           \
        TRY(call_pc, mc_registry_name(classes, block, suffix));                                    \
    } while (0)
#define META(pc) TRY(pc, mc_registry_block_metadata(classes, block))
#define NO_STATS(pc)                                                                               \
    do {                                                                                           \
        AT(pc);                                                                                    \
        mc_registry_block_no_stats(block);                                                         \
    } while (0)
#define PUBLISH(pc, index)                                                                         \
    do {                                                                                           \
        AT(pc);                                                                                    \
        classes->blocks.named[index] = block;                                                      \
        classes->named_blocks++;                                                                   \
    } while (0)

mc_registry_status mc_registry_register_blocks(mc_registry_classes *classes) {
    mc_registry_status status;
    mc_block *block;
    mc_block *source;
    mc_block_arguments arguments;
    const mc_block_sound *sound;
    const char *suffix;

    if (classes == NULL) {
        return MC_REGISTRY_INVALID_ARGUMENT;
    }

    /* 1: u. */
    NEW(192, MC_BLOCK_STONE);
    ARG(196, id, 1);
    ARG(197, texture, 1);
    CTOR(198);
    H(203, 1.5f);
    R(208, 10.0f);
    SOUND(211, h, 214);
    NAME(217, "stone", 219);
    PUBLISH(222, 0);

    /* 2: v; original checkcast wp/oj at249 changes no identity. */
    NEW(225, MC_BLOCK_GRASS);
    ARG(229, id, 2);
    CTOR(230);
    H(235, 0.6f);
    SOUND(238, g, 241);
    NAME(244, "grass", 246);
    AT(249);
    PUBLISH(252, 1);

    /* 3: w. */
    NEW(255, MC_BLOCK_DIRT);
    ARG(259, id, 3);
    ARG(260, texture, 2);
    CTOR(261);
    H(266, 0.5f);
    SOUND(269, f, 272);
    NAME(275, "dirt", 277);
    PUBLISH(280, 2);

    /* 4: x. */
    NEW(283, MC_BLOCK_BASE);
    ARG(287, id, 4);
    ARG(288, texture, 16);
    ARG(290, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(293);
    H(297, 2.0f);
    R(302, 10.0f);
    SOUND(305, h, 308);
    NAME(311, "stonebrick", 313);
    PUBLISH(316, 3);

    /* 5: y. */
    NEW(319, MC_BLOCK_BASE);
    ARG(323, id, 5);
    ARG(324, texture, 4);
    ARG(325, material, mc_materials_get(classes->options.materials, MC_MATERIAL_d));
    CTOR(328);
    H(332, 2.0f);
    R(337, 5.0f);
    SOUND(340, e, 343);
    NAME(346, "wood", 348);
    META(351);
    PUBLISH(354, 4);

    /* 6: z. */
    NEW(357, MC_BLOCK_SAPLING);
    ARG(361, id, 6);
    ARG(363, texture, 15);
    CTOR(365);
    H(369, 0.0f);
    SOUND(372, g, 375);
    NAME(378, "sapling", 380);
    META(383);
    PUBLISH(386, 5);

    /* 7: A. */
    NEW(389, MC_BLOCK_BASE);
    ARG(393, id, 7);
    ARG(395, texture, 17);
    ARG(397, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(400);
    AT(403);
    mc_registry_block_unbreakable(block);
    R(408, 6000000.0f);
    SOUND(411, h, 414);
    NAME(417, "bedrock", 419);
    NO_STATS(422);
    PUBLISH(425, 6);

    /* 8: B. */
    NEW(428, MC_BLOCK_FLOWING_FLUID);
    ARG(432, id, 8);
    ARG(434, material, mc_materials_get(classes->options.materials, MC_MATERIAL_g));
    CTOR(437);
    H(442, 100.0f);
    O(446, 3);
    NAME(449, "water", 451);
    NO_STATS(454);
    META(457);
    PUBLISH(460, 7);

    /* 9: C. */
    NEW(463, MC_BLOCK_STILL_FLUID);
    ARG(467, id, 9);
    ARG(469, material, mc_materials_get(classes->options.materials, MC_MATERIAL_g));
    CTOR(472);
    H(477, 100.0f);
    O(481, 3);
    NAME(484, "water", 486);
    NO_STATS(489);
    META(492);
    PUBLISH(495, 8);

    /* 10: D. */
    NEW(498, MC_BLOCK_FLOWING_FLUID);
    ARG(502, id, 10);
    ARG(504, material, mc_materials_get(classes->options.materials, MC_MATERIAL_h));
    CTOR(507);
    H(511, 0.0f);
    E(515, 1.0f);
    O(521, 255);
    NAME(524, "lava", 526);
    NO_STATS(529);
    META(532);
    PUBLISH(535, 9);

    /* 11: E. */
    NEW(538, MC_BLOCK_STILL_FLUID);
    ARG(542, id, 11);
    ARG(544, material, mc_materials_get(classes->options.materials, MC_MATERIAL_h));
    CTOR(547);
    H(552, 100.0f);
    E(556, 1.0f);
    O(562, 255);
    NAME(565, "lava", 567);
    NO_STATS(570);
    META(573);
    PUBLISH(576, 10);

    /* 12: F. Its concrete constructor reads ln.n/hj.n. */
    NEW(579, MC_BLOCK_SAND);
    ARG(583, id, 12);
    ARG(585, texture, 18);
    CTOR(587);
    H(592, 0.5f);
    SOUND(595, l, 598);
    NAME(601, "sand", 603);
    PUBLISH(606, 11);

    /* 13: G. */
    NEW(609, MC_BLOCK_GRAVEL);
    ARG(613, id, 13);
    ARG(615, texture, 19);
    CTOR(617);
    H(622, 0.6f);
    SOUND(625, f, 628);
    NAME(631, "gravel", 633);
    PUBLISH(636, 12);

    /* 14: H. */
    NEW(639, MC_BLOCK_ORE);
    ARG(643, id, 14);
    ARG(645, texture, 32);
    CTOR(647);
    H(652, 3.0f);
    R(657, 5.0f);
    SOUND(660, h, 663);
    NAME(666, "oreGold", 668);
    PUBLISH(671, 13);

    /* 15: I. */
    NEW(674, MC_BLOCK_ORE);
    ARG(678, id, 15);
    ARG(680, texture, 33);
    CTOR(682);
    H(687, 3.0f);
    R(692, 5.0f);
    SOUND(695, h, 698);
    NAME(701, "oreIron", 703);
    PUBLISH(706, 14);

    /* 16: J. */
    NEW(709, MC_BLOCK_ORE);
    ARG(713, id, 16);
    ARG(715, texture, 34);
    CTOR(717);
    H(722, 3.0f);
    R(727, 5.0f);
    SOUND(730, h, 733);
    NAME(736, "oreCoal", 738);
    PUBLISH(741, 15);

    /* 17: K. */
    NEW(744, MC_BLOCK_LOG);
    ARG(748, id, 17);
    CTOR(750);
    H(754, 2.0f);
    SOUND(757, e, 760);
    NAME(763, "log", 765);
    META(768);
    PUBLISH(771, 16);

    /* 18: L; checkcast bk/ar at811. */
    NEW(774, MC_BLOCK_LEAVES);
    ARG(778, id, 18);
    ARG(780, texture, 52);
    CTOR(782);
    H(787, 0.2f);
    O(791, 1);
    SOUND(794, g, 797);
    NAME(800, "leaves", 802);
    NO_STATS(805);
    META(808);
    AT(811);
    PUBLISH(814, 17);

    /* 19: M. */
    NEW(817, MC_BLOCK_SPONGE);
    ARG(821, id, 19);
    CTOR(823);
    H(828, 0.6f);
    SOUND(831, g, 834);
    NAME(837, "sponge", 839);
    PUBLISH(842, 18);

    /* 20: N. */
    NEW(845, MC_BLOCK_GLASS);
    ARG(849, id, 20);
    ARG(851, texture, 49);
    ARG(853, material, mc_materials_get(classes->options.materials, MC_MATERIAL_p));
    ARG(856, flag, false);
    CTOR(857);
    H(862, 0.3f);
    SOUND(865, j, 868);
    NAME(871, "glass", 873);
    PUBLISH(876, 19);

    /* 21: O. */
    NEW(879, MC_BLOCK_ORE);
    ARG(883, id, 21);
    ARG(885, texture, 160);
    CTOR(888);
    H(893, 3.0f);
    R(898, 5.0f);
    SOUND(901, h, 904);
    NAME(907, "oreLapis", 909);
    PUBLISH(912, 20);

    /* 22: P. */
    NEW(915, MC_BLOCK_BASE);
    ARG(919, id, 22);
    ARG(921, texture, 144);
    ARG(924, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(927);
    H(932, 3.0f);
    R(937, 5.0f);
    SOUND(940, h, 943);
    NAME(946, "blockLapis", 948);
    PUBLISH(951, 21);

    /* 23: Q. */
    NEW(954, MC_BLOCK_DISPENSER);
    ARG(958, id, 23);
    CTOR(960);
    H(965, 3.5f);
    SOUND(968, h, 971);
    NAME(974, "dispenser", 976);
    META(979);
    PUBLISH(982, 22);

    /* 24: R. */
    NEW(985, MC_BLOCK_SANDSTONE);
    ARG(989, id, 24);
    CTOR(991);
    SOUND(994, h, 997);
    H(1002, 0.8f);
    NAME(1005, "sandStone", 1007);
    PUBLISH(1010, 23);

    /* 25: S. */
    NEW(1013, MC_BLOCK_NOTE_BLOCK);
    ARG(1017, id, 25);
    CTOR(1019);
    H(1024, 0.8f);
    NAME(1027, "musicBlock", 1029);
    META(1032);
    PUBLISH(1035, 24);

    /* 26: T. */
    NEW(1038, MC_BLOCK_BED);
    ARG(1042, id, 26);
    CTOR(1044);
    H(1049, 0.2f);
    NAME(1052, "bed", 1054);
    NO_STATS(1057);
    META(1060);
    PUBLISH(1063, 25);

    /* 27: U. */
    NEW(1066, MC_BLOCK_RAIL);
    ARG(1070, id, 27);
    ARG(1072, texture, 179);
    ARG(1075, flag, true);
    CTOR(1076);
    H(1081, 0.7f);
    SOUND(1084, i, 1087);
    NAME(1090, "goldenRail", 1092);
    META(1095);
    PUBLISH(1098, 26);

    /* 28: V. */
    NEW(1101, MC_BLOCK_DETECTOR_RAIL);
    ARG(1105, id, 28);
    ARG(1107, texture, 195);
    CTOR(1110);
    H(1115, 0.7f);
    SOUND(1118, i, 1121);
    NAME(1124, "detectorRail", 1126);
    META(1129);
    PUBLISH(1132, 27);

    /* 29: W. */
    NEW(1135, MC_BLOCK_PISTON_BASE);
    ARG(1139, id, 29);
    ARG(1141, texture, 106);
    ARG(1143, flag, true);
    CTOR(1144);
    NAME(1147, "pistonStickyBase", 1149);
    META(1152);
    PUBLISH(1155, 28);

    /* 30: X. */
    NEW(1158, MC_BLOCK_COBWEB);
    ARG(1162, id, 30);
    ARG(1164, texture, 11);
    CTOR(1166);
    O(1170, 1);
    H(1175, 4.0f);
    NAME(1178, "web", 1180);
    PUBLISH(1183, 29);

    /* 31: Y; checkcast ru/kz at1212. */
    NEW(1186, MC_BLOCK_TALL_GRASS);
    ARG(1190, id, 31);
    ARG(1192, texture, 39);
    CTOR(1194);
    H(1198, 0.0f);
    SOUND(1201, g, 1204);
    NAME(1207, "tallgrass", 1209);
    AT(1212);
    PUBLISH(1215, 30);

    /* 32: Z; checkcast jb/fz at1244. */
    NEW(1218, MC_BLOCK_DEAD_BUSH);
    ARG(1222, id, 32);
    ARG(1224, texture, 55);
    CTOR(1226);
    H(1230, 0.0f);
    SOUND(1233, g, 1236);
    NAME(1239, "deadbush", 1241);
    AT(1244);
    PUBLISH(1247, 31);

    /* 33: aa. */
    NEW(1250, MC_BLOCK_PISTON_BASE);
    ARG(1254, id, 33);
    ARG(1256, texture, 107);
    ARG(1258, flag, false);
    CTOR(1259);
    NAME(1262, "pistonBase", 1264);
    META(1267);
    PUBLISH(1270, 32);

    /* 34: ab; checkcast h/g at1287. */
    NEW(1273, MC_BLOCK_PISTON_HEAD);
    ARG(1277, id, 34);
    ARG(1279, texture, 107);
    CTOR(1281);
    META(1284);
    AT(1287);
    PUBLISH(1290, 33);

    /* 35: ac; original constructor has no parameters. */
    NEW(1293, MC_BLOCK_WOOL);
    CTOR(1297);
    H(1302, 0.8f);
    SOUND(1305, k, 1308);
    NAME(1311, "cloth", 1313);
    META(1316);
    PUBLISH(1319, 34);

    /* 36: ad. */
    NEW(1322, MC_BLOCK_MOVING_PISTON);
    ARG(1326, id, 36);
    CTOR(1328);
    PUBLISH(1331, 35);

    /* 37: ae; checkcast wb/nw at1360. */
    NEW(1334, MC_BLOCK_FLOWER);
    ARG(1338, id, 37);
    ARG(1340, texture, 13);
    CTOR(1342);
    H(1346, 0.0f);
    SOUND(1349, g, 1352);
    NAME(1355, "flower", 1357);
    AT(1360);
    PUBLISH(1363, 36);

    /* 38: af; checkcast wb/nw at1392. */
    NEW(1366, MC_BLOCK_FLOWER);
    ARG(1370, id, 38);
    ARG(1372, texture, 12);
    CTOR(1374);
    H(1378, 0.0f);
    SOUND(1381, g, 1384);
    NAME(1387, "rose", 1389);
    AT(1392);
    PUBLISH(1395, 37);

    /* 39: ag; checkcast to ancestor wb/nw at1429. */
    NEW(1398, MC_BLOCK_MUSHROOM);
    ARG(1402, id, 39);
    ARG(1404, texture, 29);
    CTOR(1406);
    H(1410, 0.0f);
    SOUND(1413, g, 1416);
    E(1421, 0.125f);
    NAME(1424, "mushroom", 1426);
    AT(1429);
    PUBLISH(1432, 38);

    /* 40: ah; checkcast to ancestor wb/nw at1461. */
    NEW(1435, MC_BLOCK_MUSHROOM);
    ARG(1439, id, 40);
    ARG(1441, texture, 28);
    CTOR(1443);
    H(1447, 0.0f);
    SOUND(1450, g, 1453);
    NAME(1456, "mushroom", 1458);
    AT(1461);
    PUBLISH(1464, 39);

    /* 41: ai. */
    NEW(1467, MC_BLOCK_MINERAL_STORAGE);
    ARG(1471, id, 41);
    ARG(1473, texture, 23);
    CTOR(1475);
    H(1480, 3.0f);
    R(1485, 10.0f);
    SOUND(1488, i, 1491);
    NAME(1494, "blockGold", 1496);
    PUBLISH(1499, 40);

    /* 42: aj. */
    NEW(1502, MC_BLOCK_MINERAL_STORAGE);
    ARG(1506, id, 42);
    ARG(1508, texture, 22);
    CTOR(1510);
    H(1515, 5.0f);
    R(1520, 10.0f);
    SOUND(1523, i, 1526);
    NAME(1529, "blockIron", 1531);
    PUBLISH(1534, 41);

    /* 43: ak. */
    NEW(1537, MC_BLOCK_SLAB);
    ARG(1541, id, 43);
    ARG(1543, flag, true);
    CTOR(1544);
    H(1548, 2.0f);
    R(1553, 10.0f);
    SOUND(1556, h, 1559);
    NAME(1562, "stoneSlab", 1564);
    PUBLISH(1567, 42);

    /* 44: al. */
    NEW(1570, MC_BLOCK_SLAB);
    ARG(1574, id, 44);
    ARG(1576, flag, false);
    CTOR(1577);
    H(1581, 2.0f);
    R(1586, 10.0f);
    SOUND(1589, h, 1592);
    NAME(1595, "stoneSlab", 1597);
    PUBLISH(1600, 43);

    /* 45: am. */
    NEW(1603, MC_BLOCK_BASE);
    ARG(1607, id, 45);
    ARG(1609, texture, 7);
    ARG(1611, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(1614);
    H(1618, 2.0f);
    R(1623, 10.0f);
    SOUND(1626, h, 1629);
    NAME(1632, "brick", 1634);
    PUBLISH(1637, 44);

    /* 46: an. */
    NEW(1640, MC_BLOCK_TNT);
    ARG(1644, id, 46);
    ARG(1646, texture, 8);
    CTOR(1648);
    H(1652, 0.0f);
    SOUND(1655, g, 1658);
    NAME(1661, "tnt", 1663);
    PUBLISH(1666, 45);

    /* 47: ao. */
    NEW(1669, MC_BLOCK_BOOKSHELF);
    ARG(1673, id, 47);
    ARG(1675, texture, 35);
    CTOR(1677);
    H(1682, 1.5f);
    SOUND(1685, e, 1688);
    NAME(1691, "bookshelf", 1693);
    PUBLISH(1696, 46);

    /* 48: ap. */
    NEW(1699, MC_BLOCK_BASE);
    ARG(1703, id, 48);
    ARG(1705, texture, 36);
    ARG(1707, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(1710);
    H(1714, 2.0f);
    R(1719, 10.0f);
    SOUND(1722, h, 1725);
    NAME(1728, "stoneMoss", 1730);
    PUBLISH(1733, 47);

    /* 49: aq. */
    NEW(1736, MC_BLOCK_OBSIDIAN);
    ARG(1740, id, 49);
    ARG(1742, texture, 37);
    CTOR(1744);
    H(1749, 10.0f);
    R(1754, 2000.0f);
    SOUND(1757, h, 1760);
    NAME(1763, "obsidian", 1765);
    PUBLISH(1768, 48);

    /* 50: ar. */
    NEW(1771, MC_BLOCK_TORCH);
    ARG(1775, id, 50);
    ARG(1777, texture, 80);
    CTOR(1779);
    H(1783, 0.0f);
    E(1788, 0.9375f);
    SOUND(1791, e, 1794);
    NAME(1797, "torch", 1799);
    META(1802);
    PUBLISH(1805, 49);

    /* 51: as; checkcast yq/pt at1844. */
    NEW(1808, MC_BLOCK_FIRE);
    ARG(1812, id, 51);
    ARG(1814, texture, 31);
    CTOR(1816);
    H(1820, 0.0f);
    E(1824, 1.0f);
    SOUND(1827, e, 1830);
    NAME(1833, "fire", 1835);
    NO_STATS(1838);
    META(1841);
    AT(1844);
    PUBLISH(1847, 50);

    /* 52: at. */
    NEW(1850, MC_BLOCK_MOB_SPAWNER);
    ARG(1854, id, 52);
    ARG(1856, texture, 65);
    CTOR(1858);
    H(1863, 5.0f);
    SOUND(1866, i, 1869);
    NAME(1872, "mobSpawner", 1874);
    NO_STATS(1877);
    PUBLISH(1880, 51);

    /* 53: au; retain the actual source reference, not its ID. */
    NEW(1883, MC_BLOCK_STAIRS);
    ARG(1887, id, 53);
    ARG(1889, source_block, classes->blocks.named[4]);
    CTOR(1892);
    NAME(1895, "stairsWood", 1897);
    META(1900);
    PUBLISH(1903, 52);

    /* 54: av. */
    NEW(1906, MC_BLOCK_CHEST);
    ARG(1910, id, 54);
    CTOR(1912);
    H(1917, 2.5f);
    SOUND(1920, e, 1923);
    NAME(1926, "chest", 1928);
    META(1931);
    PUBLISH(1934, 53);

    /* 55: aw. */
    NEW(1937, MC_BLOCK_REDSTONE);
    ARG(1941, id, 55);
    ARG(1943, texture, 164);
    CTOR(1946);
    H(1950, 0.0f);
    SOUND(1953, d, 1956);
    NAME(1959, "redstoneDust", 1961);
    NO_STATS(1964);
    META(1967);
    PUBLISH(1970, 54);

    /* 56: ax. */
    NEW(1973, MC_BLOCK_ORE);
    ARG(1977, id, 56);
    ARG(1979, texture, 50);
    CTOR(1981);
    H(1986, 3.0f);
    R(1991, 5.0f);
    SOUND(1994, h, 1997);
    NAME(2000, "oreDiamond", 2002);
    PUBLISH(2005, 55);

    /* 57: ay. */
    NEW(2008, MC_BLOCK_MINERAL_STORAGE);
    ARG(2012, id, 57);
    ARG(2014, texture, 24);
    CTOR(2016);
    H(2021, 5.0f);
    R(2026, 10.0f);
    SOUND(2029, i, 2032);
    NAME(2035, "blockDiamond", 2037);
    PUBLISH(2040, 56);

    /* 58: az. */
    NEW(2043, MC_BLOCK_CRAFTING_TABLE);
    ARG(2047, id, 58);
    CTOR(2049);
    H(2054, 2.5f);
    SOUND(2057, e, 2060);
    NAME(2063, "workbench", 2065);
    PUBLISH(2068, 57);

    /* 59: aA. */
    NEW(2071, MC_BLOCK_CROPS);
    ARG(2075, id, 59);
    ARG(2077, texture, 88);
    CTOR(2079);
    H(2083, 0.0f);
    SOUND(2086, g, 2089);
    NAME(2092, "crops", 2094);
    NO_STATS(2097);
    META(2100);
    PUBLISH(2103, 58);

    /* 60: aB. */
    NEW(2106, MC_BLOCK_FARMLAND);
    ARG(2110, id, 60);
    CTOR(2112);
    H(2117, 0.6f);
    SOUND(2120, f, 2123);
    NAME(2126, "farmland", 2128);
    PUBLISH(2131, 59);

    /* 61: aC. */
    NEW(2134, MC_BLOCK_FURNACE);
    ARG(2138, id, 61);
    ARG(2140, flag, false);
    CTOR(2141);
    H(2146, 3.5f);
    SOUND(2149, h, 2152);
    NAME(2155, "furnace", 2157);
    META(2160);
    PUBLISH(2163, 60);

    /* 62: aD. */
    NEW(2166, MC_BLOCK_FURNACE);
    ARG(2170, id, 62);
    ARG(2172, flag, true);
    CTOR(2173);
    H(2178, 3.5f);
    SOUND(2181, h, 2184);
    E(2189, 0.875f);
    NAME(2192, "furnace", 2194);
    META(2197);
    PUBLISH(2200, 61);

    /* 63: aE; ldc is the yk/pq Class identity, without TileEntity initialization. */
    NEW(2203, MC_BLOCK_SIGN);
    ARG(2207, id, 63);
    ARG(2209, tile_entity_class, mc_registry_sign_class(classes));
    ARG(2211, flag, true);
    CTOR(2212);
    H(2216, 1.0f);
    SOUND(2219, e, 2222);
    NAME(2225, "sign", 2227);
    NO_STATS(2230);
    META(2233);
    PUBLISH(2236, 62);

    /* 64: aF. */
    NEW(2239, MC_BLOCK_DOOR);
    ARG(2243, id, 64);
    ARG(2245, material, mc_materials_get(classes->options.materials, MC_MATERIAL_d));
    CTOR(2248);
    H(2253, 3.0f);
    SOUND(2256, e, 2259);
    NAME(2262, "doorWood", 2264);
    NO_STATS(2267);
    META(2270);
    PUBLISH(2273, 63);

    /* 65: aG. */
    NEW(2276, MC_BLOCK_LADDER);
    ARG(2280, id, 65);
    ARG(2282, texture, 83);
    CTOR(2284);
    H(2289, 0.4f);
    SOUND(2292, e, 2295);
    NAME(2298, "ladder", 2300);
    META(2303);
    PUBLISH(2306, 64);

    /* 66: aH. */
    NEW(2309, MC_BLOCK_RAIL);
    ARG(2313, id, 66);
    ARG(2315, texture, 128);
    ARG(2318, flag, false);
    CTOR(2319);
    H(2324, 0.7f);
    SOUND(2327, i, 2330);
    NAME(2333, "rail", 2335);
    META(2338);
    PUBLISH(2341, 65);

    /* 67: aI. */
    NEW(2344, MC_BLOCK_STAIRS);
    ARG(2348, id, 67);
    ARG(2350, source_block, classes->blocks.named[3]);
    CTOR(2353);
    NAME(2356, "stairsStone", 2358);
    META(2361);
    PUBLISH(2364, 66);

    /* 68: aJ; the same yk/pq Class literal as registration63. */
    NEW(2367, MC_BLOCK_SIGN);
    ARG(2371, id, 68);
    ARG(2373, tile_entity_class, mc_registry_sign_class(classes));
    ARG(2375, flag, false);
    CTOR(2376);
    H(2380, 1.0f);
    SOUND(2383, e, 2386);
    NAME(2389, "sign", 2391);
    NO_STATS(2394);
    META(2397);
    PUBLISH(2400, 67);

    /* 69: aK. */
    NEW(2403, MC_BLOCK_LEVER);
    ARG(2407, id, 69);
    ARG(2409, texture, 96);
    CTOR(2411);
    H(2416, 0.5f);
    SOUND(2419, e, 2422);
    NAME(2425, "lever", 2427);
    META(2430);
    PUBLISH(2433, 68);

    /* 70: aL; original texture read precedes pressure-class active use. */
    NEW(2436, MC_BLOCK_PRESSURE_PLATE);
    ARG(2440, id, 70);
    AT(2442);
    source = classes->blocks.named[0];
    AT(2445);
    if (source == NULL) {
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "null Block.u texture argument", block);
    }
    arguments.texture = source->fields.bm;
    TRY(2448, mc_registry_pressure_initialize(classes));
    arguments.pressure = classes->pressure_b;
    ARG(2451, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(2454);
    H(2459, 0.5f);
    SOUND(2462, h, 2465);
    NAME(2468, "pressurePlate", 2470);
    META(2473);
    PUBLISH(2476, 69);

    /* 71: aM. */
    NEW(2479, MC_BLOCK_DOOR);
    ARG(2483, id, 71);
    ARG(2485, material, mc_materials_get(classes->options.materials, MC_MATERIAL_f));
    CTOR(2488);
    H(2493, 5.0f);
    SOUND(2496, i, 2499);
    NAME(2502, "doorIron", 2504);
    NO_STATS(2507);
    META(2510);
    PUBLISH(2513, 70);

    /* 72: aN. */
    NEW(2516, MC_BLOCK_PRESSURE_PLATE);
    ARG(2520, id, 72);
    AT(2522);
    source = classes->blocks.named[4];
    AT(2525);
    if (source == NULL) {
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "null Block.y texture argument", block);
    }
    arguments.texture = source->fields.bm;
    TRY(2528, mc_registry_pressure_initialize(classes));
    arguments.pressure = classes->pressure_a;
    ARG(2531, material, mc_materials_get(classes->options.materials, MC_MATERIAL_d));
    CTOR(2534);
    H(2539, 0.5f);
    SOUND(2542, e, 2545);
    NAME(2548, "pressurePlate", 2550);
    META(2553);
    PUBLISH(2556, 71);

    /* 73: aO. */
    NEW(2559, MC_BLOCK_REDSTONE_ORE);
    ARG(2563, id, 73);
    ARG(2565, texture, 51);
    ARG(2567, flag, false);
    CTOR(2568);
    H(2573, 3.0f);
    R(2578, 5.0f);
    SOUND(2581, h, 2584);
    NAME(2587, "oreRedstone", 2589);
    META(2592);
    PUBLISH(2595, 72);

    /* 74: aP; emission precedes hardness in the source. */
    NEW(2598, MC_BLOCK_REDSTONE_ORE);
    ARG(2602, id, 74);
    ARG(2604, texture, 51);
    ARG(2606, flag, true);
    CTOR(2607);
    E(2612, 0.625f);
    H(2617, 3.0f);
    R(2622, 5.0f);
    SOUND(2625, h, 2628);
    NAME(2631, "oreRedstone", 2633);
    META(2636);
    PUBLISH(2639, 73);

    /* 75: aQ. */
    NEW(2642, MC_BLOCK_REDSTONE_TORCH);
    ARG(2646, id, 75);
    ARG(2648, texture, 115);
    ARG(2650, flag, false);
    CTOR(2651);
    H(2655, 0.0f);
    SOUND(2658, e, 2661);
    NAME(2664, "notGate", 2666);
    META(2669);
    PUBLISH(2672, 74);

    /* 76: aR. */
    NEW(2675, MC_BLOCK_REDSTONE_TORCH);
    ARG(2679, id, 76);
    ARG(2681, texture, 99);
    ARG(2683, flag, true);
    CTOR(2684);
    H(2688, 0.0f);
    E(2693, 0.5f);
    SOUND(2696, e, 2699);
    NAME(2702, "notGate", 2704);
    META(2707);
    PUBLISH(2710, 75);

    /* 77: aS. */
    NEW(2713, MC_BLOCK_BUTTON);
    ARG(2717, id, 77);
    AT(2719);
    source = classes->blocks.named[0];
    AT(2722);
    if (source == NULL) {
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "null Block.u texture argument", block);
    }
    arguments.texture = source->fields.bm;
    CTOR(2725);
    H(2730, 0.5f);
    SOUND(2733, h, 2736);
    NAME(2739, "button", 2741);
    META(2744);
    PUBLISH(2747, 76);

    /* 78: aT. */
    NEW(2750, MC_BLOCK_SNOW_LAYER);
    ARG(2754, id, 78);
    ARG(2756, texture, 66);
    CTOR(2758);
    H(2763, 0.1f);
    SOUND(2766, k, 2769);
    NAME(2772, "snow", 2774);
    PUBLISH(2777, 77);

    /* 79: aU. */
    NEW(2780, MC_BLOCK_ICE);
    ARG(2784, id, 79);
    ARG(2786, texture, 67);
    CTOR(2788);
    H(2793, 0.5f);
    O(2797, 3);
    SOUND(2800, j, 2803);
    NAME(2806, "ice", 2808);
    PUBLISH(2811, 78);

    /* 80: aV. */
    NEW(2814, MC_BLOCK_SNOW_BLOCK);
    ARG(2818, id, 80);
    ARG(2820, texture, 66);
    CTOR(2822);
    H(2827, 0.2f);
    SOUND(2830, k, 2833);
    NAME(2836, "snow", 2838);
    PUBLISH(2841, 79);

    /* 81: aW. */
    NEW(2844, MC_BLOCK_CACTUS);
    ARG(2848, id, 81);
    ARG(2850, texture, 70);
    CTOR(2852);
    H(2857, 0.4f);
    SOUND(2860, k, 2863);
    NAME(2866, "cactus", 2868);
    PUBLISH(2871, 80);

    /* 82: aX. */
    NEW(2874, MC_BLOCK_CLAY);
    ARG(2878, id, 82);
    ARG(2880, texture, 72);
    CTOR(2882);
    H(2887, 0.6f);
    SOUND(2890, f, 2893);
    NAME(2896, "clay", 2898);
    PUBLISH(2901, 81);

    /* 83: aY. */
    NEW(2904, MC_BLOCK_SUGAR_CANE);
    ARG(2908, id, 83);
    ARG(2910, texture, 73);
    CTOR(2912);
    H(2916, 0.0f);
    SOUND(2919, g, 2922);
    NAME(2925, "reeds", 2927);
    NO_STATS(2930);
    PUBLISH(2933, 82);

    /* 84: aZ. */
    NEW(2936, MC_BLOCK_JUKEBOX);
    ARG(2940, id, 84);
    ARG(2942, texture, 74);
    CTOR(2944);
    H(2948, 2.0f);
    R(2953, 10.0f);
    SOUND(2956, h, 2959);
    NAME(2962, "jukebox", 2964);
    META(2967);
    PUBLISH(2970, 83);

    /* 85: ba. */
    NEW(2973, MC_BLOCK_FENCE);
    ARG(2977, id, 85);
    ARG(2979, texture, 4);
    CTOR(2980);
    H(2984, 2.0f);
    R(2989, 5.0f);
    SOUND(2992, e, 2995);
    NAME(2998, "fence", 3000);
    META(3003);
    PUBLISH(3006, 84);

    /* 86: bb. */
    NEW(3009, MC_BLOCK_PUMPKIN);
    ARG(3013, id, 86);
    ARG(3015, texture, 102);
    ARG(3017, flag, false);
    CTOR(3018);
    H(3022, 1.0f);
    SOUND(3025, e, 3028);
    NAME(3031, "pumpkin", 3033);
    META(3036);
    PUBLISH(3039, 85);

    /* 87: bc. */
    NEW(3042, MC_BLOCK_NETHERRACK);
    ARG(3046, id, 87);
    ARG(3048, texture, 103);
    CTOR(3050);
    H(3055, 0.4f);
    SOUND(3058, h, 3061);
    NAME(3064, "hellrock", 3066);
    PUBLISH(3069, 86);

    /* 88: bd. */
    NEW(3072, MC_BLOCK_SOUL_SAND);
    ARG(3076, id, 88);
    ARG(3078, texture, 104);
    CTOR(3080);
    H(3085, 0.5f);
    SOUND(3088, l, 3091);
    NAME(3094, "hellsand", 3096);
    PUBLISH(3099, 87);

    /* 89: be. */
    NEW(3102, MC_BLOCK_GLOWSTONE);
    ARG(3106, id, 89);
    ARG(3108, texture, 105);
    ARG(3110, material, mc_materials_get(classes->options.materials, MC_MATERIAL_e));
    CTOR(3113);
    H(3118, 0.3f);
    SOUND(3121, j, 3124);
    E(3128, 1.0f);
    NAME(3131, "lightgem", 3133);
    PUBLISH(3136, 88);

    /* 90: bf; checkcast ak/z at3171. */
    NEW(3139, MC_BLOCK_PORTAL);
    ARG(3143, id, 90);
    ARG(3145, texture, 14);
    CTOR(3147);
    H(3152, -1.0f);
    SOUND(3155, j, 3158);
    E(3163, 0.75f);
    NAME(3166, "portal", 3168);
    AT(3171);
    PUBLISH(3174, 89);

    /* 91: bg. */
    NEW(3177, MC_BLOCK_PUMPKIN);
    ARG(3181, id, 91);
    ARG(3183, texture, 102);
    ARG(3185, flag, true);
    CTOR(3186);
    H(3190, 1.0f);
    SOUND(3193, e, 3196);
    E(3200, 1.0f);
    NAME(3203, "litpumpkin", 3205);
    META(3208);
    PUBLISH(3211, 90);

    /* 92: bh. */
    NEW(3214, MC_BLOCK_CAKE);
    ARG(3218, id, 92);
    ARG(3220, texture, 121);
    CTOR(3222);
    H(3227, 0.5f);
    SOUND(3230, k, 3233);
    NAME(3236, "cake", 3238);
    NO_STATS(3241);
    META(3244);
    PUBLISH(3247, 91);

    /* 93: bi. */
    NEW(3250, MC_BLOCK_REPEATER);
    ARG(3254, id, 93);
    ARG(3256, flag, false);
    CTOR(3257);
    H(3261, 0.0f);
    SOUND(3264, e, 3267);
    NAME(3270, "diode", 3272);
    NO_STATS(3275);
    META(3278);
    PUBLISH(3281, 92);

    /* 94: bj. */
    NEW(3284, MC_BLOCK_REPEATER);
    ARG(3288, id, 94);
    ARG(3290, flag, true);
    CTOR(3291);
    H(3295, 0.0f);
    E(3300, 0.625f);
    SOUND(3303, e, 3306);
    NAME(3309, "diode", 3311);
    NO_STATS(3314);
    META(3317);
    PUBLISH(3320, 93);

    /* 95: bk. */
    NEW(3323, MC_BLOCK_LOCKED_CHEST);
    ARG(3327, id, 95);
    CTOR(3329);
    H(3333, 0.0f);
    E(3337, 1.0f);
    SOUND(3340, e, 3343);
    NAME(3346, "lockedchest", 3348);
    TRY(3352, mc_registry_block_tick(classes, block, true));
    META(3355);
    PUBLISH(3358, 94);

    /* 96: bl. */
    NEW(3361, MC_BLOCK_TRAPDOOR);
    ARG(3365, id, 96);
    ARG(3367, material, mc_materials_get(classes->options.materials, MC_MATERIAL_d));
    CTOR(3370);
    H(3375, 3.0f);
    SOUND(3378, e, 3381);
    NAME(3384, "trapdoor", 3386);
    NO_STATS(3389);
    META(3392);
    PUBLISH(3395, 95);

    return MC_REGISTRY_OK;
}

#undef PUBLISH
#undef NO_STATS
#undef META
#undef NAME
#undef SOUND
#undef O
#undef E
#undef R
#undef H
#undef CTOR
#undef ARG
#undef NEW
#undef TRY
#undef AT
