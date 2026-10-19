#include "../../block/registry/registry_internal.h"

/* gm/ej.<clinit> PCs19..2661. These are actual ordered construction and
 * property calls; the following statistics active use belongs to the driver. */

const char *mc_registry_item_static_name(size_t index)
{
    static const char *const names[] = {
        "d",  "e",  "f",  "g",  "h",  "i",  "j",  "k",  "l",  "m",  "n",  "o",  "p",  "q",
        "r",  "s",  "t",  "u",  "v",  "w",  "x",  "y",  "z",  "A",  "B",  "C",  "D",  "E",
        "F",  "G",  "H",  "I",  "J",  "K",  "L",  "M",  "N",  "O",  "P",  "Q",  "R",  "S",
        "T",  "U",  "V",  "W",  "X",  "Y",  "Z",  "aa", "ab", "ac", "ad", "ae", "af", "ag",
        "ah", "ai", "aj", "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at", "au",
        "av", "aw", "ax", "ay", "az", "aA", "aB", "aC", "aD", "aE", "aF", "aG", "aH", "aI",
        "aJ", "aK", "aL", "aM", "aN", "aO", "aP", "aQ", "aR", "aS", "aT", "aU", "aV", "aW",
        "aX", "aY", "aZ", "ba", "bb", "bc", "bd", "be"};
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : NULL;
}

#define AT(pc) mc_registry_initializer_stage(classes, (pc))
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
        TRY(pc, mc_registry_item_allocate(classes, (kind), &item));                                \
        arguments = (mc_item_arguments){0};                                                        \
    } while (0)
#define ARG(pc, field, value)                                                                      \
    do {                                                                                           \
        AT(pc);                                                                                    \
        arguments.field = (value);                                                                 \
    } while (0)
#define INTEGER(pc, index, value)                                                                  \
    do {                                                                                           \
        AT(pc);                                                                                    \
        arguments.integers[index] = (value);                                                       \
    } while (0)
#define TOOL(pc, field)                                                                            \
    do {                                                                                           \
        TRY(pc, mc_registry_item_tool_initialize(classes));                                        \
        arguments.tool = classes->items.tool_##field;                                              \
    } while (0)
#define CTOR(pc) TRY(pc, mc_registry_item_construct(classes, item, &arguments))
#define ICON(x_pc, x, y_pc, y, pc)                                                                 \
    do {                                                                                           \
        AT(x_pc);                                                                                  \
        icon_x = (x);                                                                              \
        AT(y_pc);                                                                                  \
        icon_y = (y);                                                                              \
        AT(pc);                                                                                    \
        mc_registry_item_icon(item, icon_x, icon_y);                                               \
    } while (0)
#define NAME(load_pc, text, call_pc)                                                               \
    do {                                                                                           \
        TRY(load_pc, mc_registry_literal_ascii(classes, (text), &literal));                        \
        (void)literal;                                                                             \
        TRY(call_pc, mc_registry_item_name(classes, item, (text)));                                \
    } while (0)
#define THREE_D(pc)                                                                                \
    do {                                                                                           \
        AT(pc);                                                                                    \
        mc_registry_item_flag(item);                                                               \
    } while (0)
#define CONTAINER(load_pc, index, call_pc)                                                         \
    do {                                                                                           \
        AT(load_pc);                                                                               \
        container = classes->items.named[index];                                                   \
        TRY(call_pc, mc_registry_item_container(classes, item, container));                        \
    } while (0)
#define BLOCK(load_pc, name)                                                                       \
    TRY(load_pc, mc_registry_block_static_load(classes, (name), &arguments.block))
#define BLOCK_ID(load_pc, name, read_pc)                                                           \
    do {                                                                                           \
        TRY(load_pc, mc_registry_block_static_load(classes, (name), &source));                     \
        AT(read_pc);                                                                               \
        if (source == NULL) {                                                                      \
            return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,                           \
                                    "null Item constructor Block ID argument", NULL);              \
        }                                                                                          \
        arguments.integers[0] = source->fields.bn;                                                 \
    } while (0)
#define PUBLISH(pc, index)                                                                         \
    do {                                                                                           \
        AT(pc);                                                                                    \
        classes->items.named[index] = item;                                                        \
        classes->items.named_items++;                                                              \
    } while (0)

mc_registry_status mc_registry_register_items(mc_registry_classes *classes)
{
    mc_registry_status status;
    mc_item *item;
    mc_item *container;
    mc_block *source;
    mc_item_arguments arguments;
    const mc_registry_string *literal;
    int32_t icon_x, icon_y;

    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;

    /* d: raw argument0. Class initialization precedes even that argument. */
    NEW(19, MC_ITEM_WC_NY);
    ARG(23, id, 0);
    TOOL(24, c);
    CTOR(27);
    ICON(30, 2, 31, 5, 32);
    NAME(35, "shovelIron", 37);
    PUBLISH(40, 0);

    /* e. */
    NEW(43, MC_ITEM_AU_AH);
    ARG(47, id, 1);
    TOOL(48, c);
    CTOR(51);
    ICON(54, 2, 55, 6, 57);
    NAME(60, "pickaxeIron", 62);
    PUBLISH(65, 1);

    /* f. */
    NEW(68, MC_ITEM_TA_LY);
    ARG(72, id, 2);
    TOOL(73, c);
    CTOR(76);
    ICON(79, 2, 80, 7, 82);
    NAME(85, "hatchetIron", 87);
    PUBLISH(90, 2);

    /* g. */
    NEW(93, MC_ITEM_YE_PL);
    ARG(97, id, 3);
    CTOR(98);
    ICON(101, 5, 102, 0, 103);
    NAME(106, "flintAndSteel", 108);
    PUBLISH(111, 3);

    /* h. */
    NEW(114, MC_ITEM_YW_PX);
    ARG(118, id, 4);
    INTEGER(119, 0, 4);
    ARG(120, flag, false);
    CTOR(121);
    ICON(124, 10, 126, 0, 127);
    NAME(130, "apple", 132);
    PUBLISH(135, 4);

    /* i. */
    NEW(138, MC_ITEM_QZ_KJ);
    ARG(142, id, 5);
    CTOR(143);
    ICON(146, 5, 147, 1, 148);
    NAME(151, "bow", 153);
    PUBLISH(156, 5);

    /* j. */
    NEW(159, MC_ITEM_GM_EJ);
    ARG(163, id, 6);
    CTOR(165);
    ICON(168, 5, 169, 2, 170);
    NAME(173, "arrow", 175);
    PUBLISH(178, 6);

    /* k. */
    NEW(181, MC_ITEM_RX_LC);
    ARG(185, id, 7);
    CTOR(187);
    ICON(190, 7, 192, 0, 193);
    NAME(196, "coal", 198);
    PUBLISH(201, 7);

    /* l. */
    NEW(204, MC_ITEM_GM_EJ);
    ARG(208, id, 8);
    CTOR(210);
    ICON(213, 7, 215, 3, 216);
    NAME(219, "emerald", 221);
    PUBLISH(224, 8);

    /* m. */
    NEW(227, MC_ITEM_GM_EJ);
    ARG(231, id, 9);
    CTOR(233);
    ICON(236, 7, 238, 1, 239);
    NAME(242, "ingotIron", 244);
    PUBLISH(247, 9);

    /* n. */
    NEW(250, MC_ITEM_GM_EJ);
    ARG(254, id, 10);
    CTOR(256);
    ICON(259, 7, 261, 2, 262);
    NAME(265, "ingotGold", 267);
    PUBLISH(270, 10);

    /* o. */
    NEW(273, MC_ITEM_QD_JW);
    ARG(277, id, 11);
    TOOL(279, c);
    CTOR(282);
    ICON(285, 2, 286, 4, 287);
    NAME(290, "swordIron", 292);
    PUBLISH(295, 11);

    /* p. */
    NEW(298, MC_ITEM_QD_JW);
    ARG(302, id, 12);
    TOOL(304, a);
    CTOR(307);
    ICON(310, 0, 311, 4, 312);
    NAME(315, "swordWood", 317);
    PUBLISH(320, 12);

    /* q. */
    NEW(323, MC_ITEM_WC_NY);
    ARG(327, id, 13);
    TOOL(329, a);
    CTOR(332);
    ICON(335, 0, 336, 5, 337);
    NAME(340, "shovelWood", 342);
    PUBLISH(345, 13);

    /* r. */
    NEW(348, MC_ITEM_AU_AH);
    ARG(352, id, 14);
    TOOL(354, a);
    CTOR(357);
    ICON(360, 0, 361, 6, 363);
    NAME(366, "pickaxeWood", 368);
    PUBLISH(371, 14);

    /* s. */
    NEW(374, MC_ITEM_TA_LY);
    ARG(378, id, 15);
    TOOL(380, a);
    CTOR(383);
    ICON(386, 0, 387, 7, 389);
    NAME(392, "hatchetWood", 394);
    PUBLISH(397, 15);

    /* t. */
    NEW(400, MC_ITEM_QD_JW);
    ARG(404, id, 16);
    TOOL(406, b);
    CTOR(409);
    ICON(412, 1, 413, 4, 414);
    NAME(417, "swordStone", 419);
    PUBLISH(422, 16);

    /* u. */
    NEW(425, MC_ITEM_WC_NY);
    ARG(429, id, 17);
    TOOL(431, b);
    CTOR(434);
    ICON(437, 1, 438, 5, 439);
    NAME(442, "shovelStone", 444);
    PUBLISH(447, 17);

    /* v. */
    NEW(450, MC_ITEM_AU_AH);
    ARG(454, id, 18);
    TOOL(456, b);
    CTOR(459);
    ICON(462, 1, 463, 6, 465);
    NAME(468, "pickaxeStone", 470);
    PUBLISH(473, 18);

    /* w. */
    NEW(476, MC_ITEM_TA_LY);
    ARG(480, id, 19);
    TOOL(482, b);
    CTOR(485);
    ICON(488, 1, 489, 7, 491);
    NAME(494, "hatchetStone", 496);
    PUBLISH(499, 19);

    /* x. */
    NEW(502, MC_ITEM_QD_JW);
    ARG(506, id, 20);
    TOOL(508, d);
    CTOR(511);
    ICON(514, 3, 515, 4, 516);
    NAME(519, "swordDiamond", 521);
    PUBLISH(524, 20);

    /* y. */
    NEW(527, MC_ITEM_WC_NY);
    ARG(531, id, 21);
    TOOL(533, d);
    CTOR(536);
    ICON(539, 3, 540, 5, 541);
    NAME(544, "shovelDiamond", 546);
    PUBLISH(549, 21);

    /* z. */
    NEW(552, MC_ITEM_AU_AH);
    ARG(556, id, 22);
    TOOL(558, d);
    CTOR(561);
    ICON(564, 3, 565, 6, 567);
    NAME(570, "pickaxeDiamond", 572);
    PUBLISH(575, 22);

    /* A. */
    NEW(578, MC_ITEM_TA_LY);
    ARG(582, id, 23);
    TOOL(584, d);
    CTOR(587);
    ICON(590, 3, 591, 7, 593);
    NAME(596, "hatchetDiamond", 598);
    PUBLISH(601, 23);

    /* B: ThreeD precedes the name. */
    NEW(604, MC_ITEM_GM_EJ);
    ARG(608, id, 24);
    CTOR(610);
    ICON(613, 5, 614, 3, 615);
    THREE_D(618);
    NAME(621, "stick", 623);
    PUBLISH(626, 24);

    /* C. */
    NEW(629, MC_ITEM_GM_EJ);
    ARG(633, id, 25);
    CTOR(635);
    ICON(638, 7, 640, 4, 641);
    NAME(644, "bowl", 646);
    PUBLISH(649, 25);

    /* D. */
    NEW(652, MC_ITEM_BZ_BF);
    ARG(656, id, 26);
    INTEGER(658, 0, 10);
    CTOR(660);
    ICON(663, 8, 665, 4, 666);
    NAME(669, "mushroomStew", 671);
    PUBLISH(674, 26);

    /* E. */
    NEW(677, MC_ITEM_QD_JW);
    ARG(681, id, 27);
    TOOL(683, e);
    CTOR(686);
    ICON(689, 4, 690, 4, 691);
    NAME(694, "swordGold", 696);
    PUBLISH(699, 27);

    /* F. */
    NEW(702, MC_ITEM_WC_NY);
    ARG(706, id, 28);
    TOOL(708, e);
    CTOR(711);
    ICON(714, 4, 715, 5, 716);
    NAME(719, "shovelGold", 721);
    PUBLISH(724, 28);

    /* G. */
    NEW(727, MC_ITEM_AU_AH);
    ARG(731, id, 29);
    TOOL(733, e);
    CTOR(736);
    ICON(739, 4, 740, 6, 742);
    NAME(745, "pickaxeGold", 747);
    PUBLISH(750, 29);

    /* H. */
    NEW(753, MC_ITEM_TA_LY);
    ARG(757, id, 30);
    TOOL(759, e);
    CTOR(762);
    ICON(765, 4, 766, 7, 768);
    NAME(771, "hatchetGold", 773);
    PUBLISH(776, 30);

    /* I. */
    NEW(779, MC_ITEM_GM_EJ);
    ARG(783, id, 31);
    CTOR(785);
    ICON(788, 8, 790, 0, 791);
    NAME(794, "string", 796);
    PUBLISH(799, 31);

    /* J. */
    NEW(802, MC_ITEM_GM_EJ);
    ARG(806, id, 32);
    CTOR(808);
    ICON(811, 8, 813, 1, 814);
    NAME(817, "feather", 819);
    PUBLISH(822, 32);

    /* K. */
    NEW(825, MC_ITEM_GM_EJ);
    ARG(829, id, 33);
    CTOR(831);
    ICON(834, 8, 836, 2, 837);
    NAME(840, "sulphur", 842);
    PUBLISH(845, 33);

    /* L. */
    NEW(848, MC_ITEM_LA_GZ);
    ARG(852, id, 34);
    TOOL(854, a);
    CTOR(857);
    ICON(860, 0, 861, 8, 863);
    NAME(866, "hoeWood", 868);
    PUBLISH(871, 34);

    /* M. */
    NEW(874, MC_ITEM_LA_GZ);
    ARG(878, id, 35);
    TOOL(880, b);
    CTOR(883);
    ICON(886, 1, 887, 8, 889);
    NAME(892, "hoeStone", 894);
    PUBLISH(897, 35);

    /* N. */
    NEW(900, MC_ITEM_LA_GZ);
    ARG(904, id, 36);
    TOOL(906, c);
    CTOR(909);
    ICON(912, 2, 913, 8, 915);
    NAME(918, "hoeIron", 920);
    PUBLISH(923, 36);

    /* O. */
    NEW(926, MC_ITEM_LA_GZ);
    ARG(930, id, 37);
    TOOL(932, d);
    CTOR(935);
    ICON(938, 3, 939, 8, 941);
    NAME(944, "hoeDiamond", 946);
    PUBLISH(949, 37);

    /* P. */
    NEW(952, MC_ITEM_LA_GZ);
    ARG(956, id, 38);
    TOOL(958, e);
    CTOR(961);
    ICON(964, 4, 965, 8, 967);
    NAME(970, "hoeGold", 972);
    PUBLISH(975, 38);

    /* Q: read the actual Block.aA.bn after this Item's RAW allocation. */
    NEW(978, MC_ITEM_RK_KR);
    ARG(982, id, 39);
    BLOCK_ID(984, "aA", 987);
    CTOR(990);
    ICON(993, 9, 995, 0, 996);
    NAME(999, "seeds", 1001);
    PUBLISH(1004, 39);

    /* R. */
    NEW(1007, MC_ITEM_GM_EJ);
    ARG(1011, id, 40);
    CTOR(1013);
    ICON(1016, 9, 1018, 1, 1019);
    NAME(1022, "wheat", 1024);
    PUBLISH(1027, 40);

    /* S. */
    NEW(1030, MC_ITEM_YW_PX);
    ARG(1034, id, 41);
    INTEGER(1036, 0, 5);
    ARG(1037, flag, false);
    CTOR(1038);
    ICON(1041, 9, 1043, 2, 1044);
    NAME(1047, "bread", 1049);
    PUBLISH(1052, 41);

    /* T. */
    NEW(1055, MC_ITEM_WA_NX);
    ARG(1059, id, 42);
    INTEGER(1061, 0, 0);
    INTEGER(1062, 1, 0);
    INTEGER(1063, 2, 0);
    CTOR(1064);
    ICON(1067, 0, 1068, 0, 1069);
    NAME(1072, "helmetCloth", 1074);
    PUBLISH(1077, 42);

    /* U. */
    NEW(1080, MC_ITEM_WA_NX);
    ARG(1084, id, 43);
    INTEGER(1086, 0, 0);
    INTEGER(1087, 1, 0);
    INTEGER(1088, 2, 1);
    CTOR(1089);
    ICON(1092, 0, 1093, 1, 1094);
    NAME(1097, "chestplateCloth", 1099);
    PUBLISH(1102, 43);

    /* V. */
    NEW(1105, MC_ITEM_WA_NX);
    ARG(1109, id, 44);
    INTEGER(1111, 0, 0);
    INTEGER(1112, 1, 0);
    INTEGER(1113, 2, 2);
    CTOR(1114);
    ICON(1117, 0, 1118, 2, 1119);
    NAME(1122, "leggingsCloth", 1124);
    PUBLISH(1127, 44);

    /* W. */
    NEW(1130, MC_ITEM_WA_NX);
    ARG(1134, id, 45);
    INTEGER(1136, 0, 0);
    INTEGER(1137, 1, 0);
    INTEGER(1138, 2, 3);
    CTOR(1139);
    ICON(1142, 0, 1143, 3, 1144);
    NAME(1147, "bootsCloth", 1149);
    PUBLISH(1152, 45);

    /* X. */
    NEW(1155, MC_ITEM_WA_NX);
    ARG(1159, id, 46);
    INTEGER(1161, 0, 1);
    INTEGER(1162, 1, 1);
    INTEGER(1163, 2, 0);
    CTOR(1164);
    ICON(1167, 1, 1168, 0, 1169);
    NAME(1172, "helmetChain", 1174);
    PUBLISH(1177, 46);

    /* Y. */
    NEW(1180, MC_ITEM_WA_NX);
    ARG(1184, id, 47);
    INTEGER(1186, 0, 1);
    INTEGER(1187, 1, 1);
    INTEGER(1188, 2, 1);
    CTOR(1189);
    ICON(1192, 1, 1193, 1, 1194);
    NAME(1197, "chestplateChain", 1199);
    PUBLISH(1202, 47);

    /* Z. */
    NEW(1205, MC_ITEM_WA_NX);
    ARG(1209, id, 48);
    INTEGER(1211, 0, 1);
    INTEGER(1212, 1, 1);
    INTEGER(1213, 2, 2);
    CTOR(1214);
    ICON(1217, 1, 1218, 2, 1219);
    NAME(1222, "leggingsChain", 1224);
    PUBLISH(1227, 48);

    /* aa. */
    NEW(1230, MC_ITEM_WA_NX);
    ARG(1234, id, 49);
    INTEGER(1236, 0, 1);
    INTEGER(1237, 1, 1);
    INTEGER(1238, 2, 3);
    CTOR(1239);
    ICON(1242, 1, 1243, 3, 1244);
    NAME(1247, "bootsChain", 1249);
    PUBLISH(1252, 49);

    /* ab. */
    NEW(1255, MC_ITEM_WA_NX);
    ARG(1259, id, 50);
    INTEGER(1261, 0, 2);
    INTEGER(1262, 1, 2);
    INTEGER(1263, 2, 0);
    CTOR(1264);
    ICON(1267, 2, 1268, 0, 1269);
    NAME(1272, "helmetIron", 1274);
    PUBLISH(1277, 50);

    /* ac. */
    NEW(1280, MC_ITEM_WA_NX);
    ARG(1284, id, 51);
    INTEGER(1286, 0, 2);
    INTEGER(1287, 1, 2);
    INTEGER(1288, 2, 1);
    CTOR(1289);
    ICON(1292, 2, 1293, 1, 1294);
    NAME(1297, "chestplateIron", 1299);
    PUBLISH(1302, 51);

    /* ad. */
    NEW(1305, MC_ITEM_WA_NX);
    ARG(1309, id, 52);
    INTEGER(1311, 0, 2);
    INTEGER(1312, 1, 2);
    INTEGER(1313, 2, 2);
    CTOR(1314);
    ICON(1317, 2, 1318, 2, 1319);
    NAME(1322, "leggingsIron", 1324);
    PUBLISH(1327, 52);

    /* ae. */
    NEW(1330, MC_ITEM_WA_NX);
    ARG(1334, id, 53);
    INTEGER(1336, 0, 2);
    INTEGER(1337, 1, 2);
    INTEGER(1338, 2, 3);
    CTOR(1339);
    ICON(1342, 2, 1343, 3, 1344);
    NAME(1347, "bootsIron", 1349);
    PUBLISH(1352, 53);

    /* af. */
    NEW(1355, MC_ITEM_WA_NX);
    ARG(1359, id, 54);
    INTEGER(1361, 0, 3);
    INTEGER(1362, 1, 3);
    INTEGER(1363, 2, 0);
    CTOR(1364);
    ICON(1367, 3, 1368, 0, 1369);
    NAME(1372, "helmetDiamond", 1374);
    PUBLISH(1377, 54);

    /* ag. */
    NEW(1380, MC_ITEM_WA_NX);
    ARG(1384, id, 55);
    INTEGER(1386, 0, 3);
    INTEGER(1387, 1, 3);
    INTEGER(1388, 2, 1);
    CTOR(1389);
    ICON(1392, 3, 1393, 1, 1394);
    NAME(1397, "chestplateDiamond", 1399);
    PUBLISH(1402, 55);

    /* ah. */
    NEW(1405, MC_ITEM_WA_NX);
    ARG(1409, id, 56);
    INTEGER(1411, 0, 3);
    INTEGER(1412, 1, 3);
    INTEGER(1413, 2, 2);
    CTOR(1414);
    ICON(1417, 3, 1418, 2, 1419);
    NAME(1422, "leggingsDiamond", 1424);
    PUBLISH(1427, 56);

    /* ai. */
    NEW(1430, MC_ITEM_WA_NX);
    ARG(1434, id, 57);
    INTEGER(1436, 0, 3);
    INTEGER(1437, 1, 3);
    INTEGER(1438, 2, 3);
    CTOR(1439);
    ICON(1442, 3, 1443, 3, 1444);
    NAME(1447, "bootsDiamond", 1449);
    PUBLISH(1452, 57);

    /* aj: the first remaining argument is1, not the render value4. */
    NEW(1455, MC_ITEM_WA_NX);
    ARG(1459, id, 58);
    INTEGER(1461, 0, 1);
    INTEGER(1462, 1, 4);
    INTEGER(1463, 2, 0);
    CTOR(1464);
    ICON(1467, 4, 1468, 0, 1469);
    NAME(1472, "helmetGold", 1474);
    PUBLISH(1477, 58);

    /* ak. */
    NEW(1480, MC_ITEM_WA_NX);
    ARG(1484, id, 59);
    INTEGER(1486, 0, 1);
    INTEGER(1487, 1, 4);
    INTEGER(1488, 2, 1);
    CTOR(1489);
    ICON(1492, 4, 1493, 1, 1494);
    NAME(1497, "chestplateGold", 1499);
    PUBLISH(1502, 59);

    /* al. */
    NEW(1505, MC_ITEM_WA_NX);
    ARG(1509, id, 60);
    INTEGER(1511, 0, 1);
    INTEGER(1512, 1, 4);
    INTEGER(1513, 2, 2);
    CTOR(1514);
    ICON(1517, 4, 1518, 2, 1519);
    NAME(1522, "leggingsGold", 1524);
    PUBLISH(1527, 60);

    /* am. */
    NEW(1530, MC_ITEM_WA_NX);
    ARG(1534, id, 61);
    INTEGER(1536, 0, 1);
    INTEGER(1537, 1, 4);
    INTEGER(1538, 2, 3);
    CTOR(1539);
    ICON(1542, 4, 1543, 3, 1544);
    NAME(1547, "bootsGold", 1549);
    PUBLISH(1552, 61);

    /* an. */
    NEW(1555, MC_ITEM_GM_EJ);
    ARG(1559, id, 62);
    CTOR(1561);
    ICON(1564, 6, 1566, 0, 1567);
    NAME(1570, "flint", 1572);
    PUBLISH(1575, 62);

    /* ao. */
    NEW(1578, MC_ITEM_YW_PX);
    ARG(1582, id, 63);
    INTEGER(1584, 0, 3);
    ARG(1585, flag, true);
    CTOR(1586);
    ICON(1589, 7, 1591, 5, 1592);
    NAME(1595, "porkchopRaw", 1597);
    PUBLISH(1600, 63);

    /* ap. */
    NEW(1603, MC_ITEM_YW_PX);
    ARG(1607, id, 64);
    INTEGER(1609, 0, 8);
    ARG(1611, flag, true);
    CTOR(1612);
    ICON(1615, 8, 1617, 5, 1618);
    NAME(1621, "porkchopCooked", 1623);
    PUBLISH(1626, 64);

    /* aq. */
    NEW(1629, MC_ITEM_YM_PS);
    ARG(1633, id, 65);
    CTOR(1635);
    ICON(1638, 10, 1640, 1, 1641);
    NAME(1644, "painting", 1646);
    PUBLISH(1649, 65);

    /* ar. */
    NEW(1652, MC_ITEM_YW_PX);
    ARG(1656, id, 66);
    INTEGER(1658, 0, 42);
    ARG(1660, flag, false);
    CTOR(1661);
    ICON(1664, 11, 1666, 0, 1667);
    NAME(1670, "appleGold", 1672);
    PUBLISH(1675, 66);

    /* as. */
    NEW(1678, MC_ITEM_VC_NG);
    ARG(1682, id, 67);
    CTOR(1684);
    ICON(1687, 10, 1689, 2, 1690);
    NAME(1693, "sign", 1695);
    PUBLISH(1698, 67);

    /* at. */
    NEW(1701, MC_ITEM_HU_FG);
    ARG(1705, id, 68);
    ARG(1707, material, mc_materials_get(classes->options.materials, MC_MATERIAL_d));
    CTOR(1710);
    ICON(1713, 11, 1715, 2, 1716);
    NAME(1719, "doorWood", 1721);
    PUBLISH(1724, 68);

    /* au: the empty contents argument is a literal zero. */
    NEW(1727, MC_ITEM_BC_AL);
    ARG(1731, id, 69);
    INTEGER(1733, 0, 0);
    CTOR(1734);
    ICON(1737, 10, 1739, 4, 1740);
    NAME(1743, "bucket", 1745);
    PUBLISH(1748, 69);

    /* av: read the current au only after name construction. */
    NEW(1751, MC_ITEM_BC_AL);
    ARG(1755, id, 70);
    BLOCK_ID(1757, "B", 1760);
    CTOR(1763);
    ICON(1766, 11, 1768, 4, 1769);
    NAME(1772, "bucketWater", 1774);
    CONTAINER(1777, 69, 1780);
    PUBLISH(1783, 70);

    /* aw. */
    NEW(1786, MC_ITEM_BC_AL);
    ARG(1790, id, 71);
    BLOCK_ID(1792, "D", 1795);
    CTOR(1798);
    ICON(1801, 12, 1803, 4, 1804);
    NAME(1807, "bucketLava", 1809);
    CONTAINER(1812, 69, 1815);
    PUBLISH(1818, 71);

    /* ax. */
    NEW(1821, MC_ITEM_RL_KS);
    ARG(1825, id, 72);
    INTEGER(1827, 0, 0);
    CTOR(1828);
    ICON(1831, 7, 1833, 8, 1835);
    NAME(1838, "minecart", 1840);
    PUBLISH(1843, 72);

    /* ay. */
    NEW(1846, MC_ITEM_SA_LG);
    ARG(1850, id, 73);
    CTOR(1852);
    ICON(1855, 8, 1857, 6, 1859);
    NAME(1862, "saddle", 1864);
    PUBLISH(1867, 73);

    /* az. */
    NEW(1870, MC_ITEM_HU_FG);
    ARG(1874, id, 74);
    ARG(1876, material, mc_materials_get(classes->options.materials, MC_MATERIAL_f));
    CTOR(1879);
    ICON(1882, 12, 1884, 2, 1885);
    NAME(1888, "doorIron", 1890);
    PUBLISH(1893, 74);

    /* aA. */
    NEW(1896, MC_ITEM_IE_FK);
    ARG(1900, id, 75);
    CTOR(1902);
    ICON(1905, 8, 1907, 3, 1908);
    NAME(1911, "redstone", 1913);
    PUBLISH(1916, 75);

    /* aB. */
    NEW(1919, MC_ITEM_DM_CF);
    ARG(1923, id, 76);
    CTOR(1925);
    ICON(1928, 14, 1930, 0, 1931);
    NAME(1934, "snowball", 1936);
    PUBLISH(1939, 76);

    /* aC. */
    NEW(1942, MC_ITEM_VD_NI);
    ARG(1946, id, 77);
    CTOR(1948);
    ICON(1951, 8, 1953, 8, 1955);
    NAME(1958, "boat", 1960);
    PUBLISH(1963, 77);

    /* aD. */
    NEW(1966, MC_ITEM_GM_EJ);
    ARG(1970, id, 78);
    CTOR(1972);
    ICON(1975, 7, 1977, 6, 1979);
    NAME(1982, "leather", 1984);
    PUBLISH(1987, 78);

    /* aE: milk has the literal contents argument -1. */
    NEW(1990, MC_ITEM_BC_AL);
    ARG(1994, id, 79);
    INTEGER(1996, 0, -1);
    CTOR(1997);
    ICON(2000, 13, 2002, 4, 2003);
    NAME(2006, "milk", 2008);
    CONTAINER(2011, 69, 2014);
    PUBLISH(2017, 79);

    /* aF. */
    NEW(2020, MC_ITEM_GM_EJ);
    ARG(2024, id, 80);
    CTOR(2026);
    ICON(2029, 6, 2031, 1, 2032);
    NAME(2035, "brick", 2037);
    PUBLISH(2040, 80);

    /* aG. */
    NEW(2043, MC_ITEM_GM_EJ);
    ARG(2047, id, 81);
    CTOR(2049);
    ICON(2052, 9, 2054, 3, 2055);
    NAME(2058, "clay", 2060);
    PUBLISH(2063, 81);

    /* aH: retain the actual Block argument until its constructor reads bn. */
    NEW(2066, MC_ITEM_LV_HN);
    ARG(2070, id, 82);
    BLOCK(2072, "aY");
    CTOR(2075);
    ICON(2078, 11, 2080, 1, 2081);
    NAME(2084, "reeds", 2086);
    PUBLISH(2089, 82);

    /* aI. */
    NEW(2092, MC_ITEM_GM_EJ);
    ARG(2096, id, 83);
    CTOR(2098);
    ICON(2101, 10, 2103, 3, 2104);
    NAME(2107, "paper", 2109);
    PUBLISH(2112, 83);

    /* aJ. */
    NEW(2115, MC_ITEM_GM_EJ);
    ARG(2119, id, 84);
    CTOR(2121);
    ICON(2124, 11, 2126, 3, 2127);
    NAME(2130, "book", 2132);
    PUBLISH(2135, 84);

    /* aK. */
    NEW(2138, MC_ITEM_GM_EJ);
    ARG(2142, id, 85);
    CTOR(2144);
    ICON(2147, 14, 2149, 1, 2150);
    NAME(2153, "slimeball", 2155);
    PUBLISH(2158, 85);

    /* aL. */
    NEW(2161, MC_ITEM_RL_KS);
    ARG(2165, id, 86);
    INTEGER(2167, 0, 1);
    CTOR(2168);
    ICON(2171, 7, 2173, 9, 2175);
    NAME(2178, "minecartChest", 2180);
    PUBLISH(2183, 86);

    /* aM. */
    NEW(2186, MC_ITEM_RL_KS);
    ARG(2190, id, 87);
    INTEGER(2192, 0, 2);
    CTOR(2193);
    ICON(2196, 7, 2198, 10, 2200);
    NAME(2203, "minecartFurnace", 2205);
    PUBLISH(2208, 87);

    /* aN. */
    NEW(2211, MC_ITEM_MO_HX);
    ARG(2215, id, 88);
    CTOR(2217);
    ICON(2220, 12, 2222, 0, 2223);
    NAME(2226, "egg", 2228);
    PUBLISH(2231, 88);

    /* aO. */
    NEW(2234, MC_ITEM_GM_EJ);
    ARG(2238, id, 89);
    CTOR(2240);
    ICON(2243, 6, 2245, 3, 2246);
    NAME(2249, "compass", 2251);
    PUBLISH(2254, 89);

    /* aP. */
    NEW(2257, MC_ITEM_CS_BV);
    ARG(2261, id, 90);
    CTOR(2263);
    ICON(2266, 5, 2267, 4, 2268);
    NAME(2271, "fishingRod", 2273);
    PUBLISH(2276, 90);

    /* aQ. */
    NEW(2279, MC_ITEM_GM_EJ);
    ARG(2283, id, 91);
    CTOR(2285);
    ICON(2288, 6, 2290, 4, 2291);
    NAME(2294, "clock", 2296);
    PUBLISH(2299, 91);

    /* aR. */
    NEW(2302, MC_ITEM_GM_EJ);
    ARG(2306, id, 92);
    CTOR(2308);
    ICON(2311, 9, 2313, 4, 2314);
    NAME(2317, "yellowDust", 2319);
    PUBLISH(2322, 92);

    /* aS. */
    NEW(2325, MC_ITEM_YW_PX);
    ARG(2329, id, 93);
    INTEGER(2331, 0, 2);
    ARG(2332, flag, false);
    CTOR(2333);
    ICON(2336, 9, 2338, 5, 2339);
    NAME(2342, "fishRaw", 2344);
    PUBLISH(2347, 93);

    /* aT. */
    NEW(2350, MC_ITEM_YW_PX);
    ARG(2354, id, 94);
    INTEGER(2356, 0, 5);
    ARG(2357, flag, false);
    CTOR(2358);
    ICON(2361, 10, 2363, 5, 2364);
    NAME(2367, "fishCooked", 2369);
    PUBLISH(2372, 94);

    /* aU. */
    NEW(2375, MC_ITEM_KM_GU);
    ARG(2379, id, 95);
    CTOR(2381);
    ICON(2384, 14, 2386, 4, 2387);
    NAME(2390, "dyePowder", 2392);
    PUBLISH(2395, 95);

    /* aV: unlike stick, ThreeD follows the name. */
    NEW(2398, MC_ITEM_GM_EJ);
    ARG(2402, id, 96);
    CTOR(2404);
    ICON(2407, 12, 2409, 1, 2410);
    NAME(2413, "bone", 2415);
    THREE_D(2418);
    PUBLISH(2421, 96);

    /* aW. */
    NEW(2424, MC_ITEM_GM_EJ);
    ARG(2428, id, 97);
    CTOR(2430);
    ICON(2433, 13, 2435, 0, 2436);
    NAME(2439, "sugar", 2441);
    THREE_D(2444);
    PUBLISH(2447, 97);

    /* aX. */
    NEW(2450, MC_ITEM_LV_HN);
    ARG(2454, id, 98);
    BLOCK(2456, "bh");
    CTOR(2459);
    AT(2462);
    icon_x = 1;
    AT(2463);
    mc_registry_item_stack_limit(item, icon_x);
    ICON(2466, 13, 2468, 1, 2469);
    NAME(2472, "cake", 2474);
    PUBLISH(2477, 98);

    /* aY. */
    NEW(2480, MC_ITEM_LP_HK);
    ARG(2484, id, 99);
    CTOR(2486);
    AT(2489);
    icon_x = 1;
    AT(2490);
    mc_registry_item_stack_limit(item, icon_x);
    ICON(2493, 13, 2495, 2, 2496);
    NAME(2499, "bed", 2501);
    PUBLISH(2504, 99);

    /* aZ. */
    NEW(2507, MC_ITEM_LV_HN);
    ARG(2511, id, 100);
    BLOCK(2513, "bi");
    CTOR(2516);
    ICON(2519, 6, 2521, 5, 2522);
    NAME(2525, "diode", 2527);
    PUBLISH(2530, 100);

    /* ba: descriptor(IIZI), with the boolean between the two extra ints. */
    NEW(2533, MC_ITEM_QK_JY);
    ARG(2537, id, 101);
    INTEGER(2539, 0, 1);
    ARG(2540, flag, false);
    INTEGER(2541, 1, 8);
    CTOR(2543);
    ICON(2546, 12, 2548, 5, 2549);
    NAME(2552, "cookie", 2554);
    PUBLISH(2557, 101);

    /* bb: actual checkcast wr/ol at2580 preserves this same owner. */
    NEW(2560, MC_ITEM_WR_OL);
    ARG(2564, id, 102);
    CTOR(2566);
    ICON(2569, 12, 2571, 3, 2572);
    NAME(2575, "map", 2577);
    AT(2580);
    PUBLISH(2583, 102);

    /* bc: actual checkcast bl/as at2606. */
    NEW(2586, MC_ITEM_BL_AS);
    ARG(2590, id, 103);
    CTOR(2592);
    ICON(2595, 13, 2597, 5, 2598);
    NAME(2601, "shears", 2603);
    AT(2606);
    PUBLISH(2609, 103);

    /* bd: the constructor's String differs from its later base name. */
    NEW(2612, MC_ITEM_TR_MJ);
    ARG(2616, id, 2000);
    TRY(2619, mc_registry_literal_ascii(classes, "13", &arguments.text));
    CTOR(2621);
    ICON(2624, 0, 2625, 15, 2627);
    NAME(2630, "record", 2632);
    PUBLISH(2635, 104);

    /* be. */
    NEW(2638, MC_ITEM_TR_MJ);
    ARG(2642, id, 2001);
    TRY(2645, mc_registry_literal_ascii(classes, "cat", &arguments.text));
    CTOR(2647);
    ICON(2650, 1, 2651, 15, 2653);
    NAME(2656, "record", 2658);
    PUBLISH(2661, 105);

    return MC_REGISTRY_OK;
}

#undef PUBLISH
#undef BLOCK_ID
#undef BLOCK
#undef CONTAINER
#undef THREE_D
#undef NAME
#undef ICON
#undef CTOR
#undef TOOL
#undef INTEGER
#undef ARG
#undef NEW
#undef TRY
#undef AT
