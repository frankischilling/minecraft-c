import java.lang.reflect.Field;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;

/** Declared fields and genuine observer callbacks around unchanged YE/IZ/OB. */
@SuppressWarnings({"unchecked", "rawtypes"})
public final class FlintAndSteelObservation {
    static final String[] KEYS = {"mode", "operation", "xyz", "face", "stack_id", "count", "damage", "animation", "world_seed", "item_seed", "remote", "null_world", "null_player", "null_stack", "null_item_random", "null_fire", "null_provider", "null_access", "null_stats_writer", "null_use_stats", "null_break_stats", "null_item_table", "item_random_world", "sound_action", "cell_error_at", "range_error_at", "sound_error_at", "cells", "heights"};
    static final String[] DATA = {
        "flint-face-0|direct|15,64,15|0|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,62,15,1,0;15,64,15,1,0|",
        "flint-face-1|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-face-2|direct|15,64,15|2|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,14,1,0;15,64,15,1,0|",
        "flint-face-3|direct|15,64,15|3|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,16,1,0;15,64,15,1,0|",
        "flint-face-4|direct|15,64,15|4|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|14,63,15,1,0;15,64,15,1,0|",
        "flint-face-5|direct|15,64,15|5|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0;16,63,15,1,0|",
        "flint-wrapper|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-face-unchanged--1|direct|15,64,15|-1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,15,1,0;15,64,15,1,0|",
        "flint-face-unchanged-6|direct|15,64,15|6|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,15,1,0;15,64,15,1,0|",
        "flint-face-unchanged-2147483647|direct|15,64,15|2147483647|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,15,1,0;15,64,15,1,0|",
        "flint-direct-occupied|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0;15,65,15,1,7|",
        "flint-direct-unsupported|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrapper-occupied|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0;15,65,15,1,7|",
        "flint-wrapper-unsupported|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-ob-occupied|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0;15,65,15,1,7|",
        "flint-ob-unsupported|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-direct-portal-x|direct|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|14,64,15,49,0;14,65,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "flint-direct-portal-z|direct|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,15,49,0;15,63,16,49,0;15,64,14,49,0;15,64,17,49,0;15,65,14,49,0;15,65,17,49,0;15,66,14,49,0;15,66,17,49,0;15,67,15,49,0;15,67,16,49,0|",
        "flint-wrapper-portal-x|wrapper|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|14,64,15,49,0;14,65,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "flint-wrapper-portal-z|wrapper|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,15,49,0;15,63,16,49,0;15,64,14,49,0;15,64,17,49,0;15,65,14,49,0;15,65,17,49,0;15,66,14,49,0;15,66,17,49,0;15,67,15,49,0;15,67,16,49,0|",
        "flint-ob-portal-x|ob|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|14,64,15,49,0;14,65,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "flint-ob-portal-z|ob|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,63,15,49,0;15,63,16,49,0;15,64,14,49,0;15,64,17,49,0;15,65,14,49,0;15,65,17,49,0;15,66,14,49,0;15,66,17,49,0;15,67,15,49,0;15,67,16,49,0|",
        "flint-portal-cell-error-third|direct|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|3|0|0|14,64,15,49,0;14,65,15,49,0;14,66,15,49,0;15,63,15,49,0;15,67,15,49,0;16,63,15,49,0;16,67,15,49,0;17,64,15,49,0;17,65,15,49,0;17,66,15,49,0|",
        "flint-portal-range-error-second|direct|15,63,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|2|0|15,63,15,49,0;15,63,16,49,0;15,64,14,49,0;15,64,17,49,0;15,65,14,49,0;15,65,17,49,0;15,66,14,49,0;15,66,17,49,0;15,67,15,49,0;15,67,16,49,0|",
        "flint-remote-supported|direct|15,64,15|1|259|1|0|5|1|2|true|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-remote-unsupported|direct|15,64,15|1|259|1|0|5|1|2|true|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrapper-remote|wrapper|15,64,15|1|259|1|0|5|1|2|true|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrap-x-max|direct|2147483647,64,15|5|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrap-x-min|direct|-2147483648,64,15|4|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrap-y-max|direct|15,2147483647,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrap-y-min|direct|15,-2147483648,15|0|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrap-z-max|direct|15,64,2147483647|3|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-wrap-z-min|direct|15,64,-2147483648|2|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-vertical--2|direct|15,-2,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-vertical--1|direct|15,-1,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0||",
        "flint-vertical-127|direct|15,127,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,127,15,1,0|",
        "flint-provider-bound-x-in|direct|31999999,64,15|6|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0||",
        "flint-provider-bound-x-out|direct|32000000,64,15|6|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0||",
        "flint-provider-bound-z-in|direct|15,64,32000000|6|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0||",
        "flint-provider-bound-z-out|direct|15,64,32000001|6|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0||",
        "flint-provider-bound-x-low-in|direct|-32000000,64,15|6|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0||",
        "flint-provider-bound-x-low-out|direct|-32000001,64,15|6|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0||",
        "flint-direct-world-null|direct|15,64,15|1|259|1|0|5|1|2|false|true|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-direct-stack-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|true|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-direct-provider-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-world-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|true|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-stack-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|true|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-provider-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob-world-null|ob|15,64,15|1|259|1|0|5|1|2|false|true|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob-stack-null|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|true|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob-provider-null|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|true|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-player-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|true|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-player-null-break|direct|15,64,15|1|259|1|64|5|1|2|false|false|true|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-player-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|true|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-player-null-break|wrapper|15,64,15|1|259|1|64|5|1|2|false|false|true|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob-player-null|ob|15,64,15|1|259|1|0|5|1|2|false|false|true|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-random-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|true|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-random-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|true|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-fire-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|true|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-fire-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|true|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-random-before-fire-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|true|true|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-occupied-skips-random-fire|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|true|true|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0;15,65,15,1,7|",
        "flint-access-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|true|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-occupied-skips-access|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|true|false|false|false|false|false|none|0|0|0|15,64,15,1,0;15,65,15,1,7|",
        "flint-writer-null-unused|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|true|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-writer-null-break|direct|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|true|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-writer-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|true|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-writer-null-break|wrapper|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|true|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob-writer-null|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|true|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-use-array-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|true|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-break-array-null|direct|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|false|false|true|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-use-array-before-player-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|true|false|false|false|false|false|false|true|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-item-table-null|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|true|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-item-table-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|true|false|none|0|0|0|15,64,15,1,0|",
        "flint-direct-sound-error|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|1|15,64,15,1,0|",
        "flint-direct-range-error|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|1|0|15,64,15,1,0|",
        "flint-direct-cell-error|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|1|0|0|15,64,15,1,0|",
        "flint-wrapper-sound-error|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|1|15,64,15,1,0|",
        "flint-wrapper-range-error|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|1|0|15,64,15,1,0|",
        "flint-wrapper-cell-error|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|1|0|0|15,64,15,1,0|",
        "flint-ob-sound-error|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|1|15,64,15,1,0|",
        "flint-ob-range-error|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|1|0|15,64,15,1,0|",
        "flint-ob-cell-error|ob|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|1|0|0|15,64,15,1,0|",
        "flint-sound-fire-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|fire-null|0|0|0|15,64,15,1,0|",
        "flint-sound-fire-id-zero|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|fire-id-zero|0|0|0|15,64,15,1,0|",
        "flint-sound-item-random-world|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|item-random-world|0|0|0|15,64,15,1,0|",
        "flint-sound-stack-stick|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|stack-stick|0|0|0|15,64,15,1,0|",
        "flint-sound-stats-writer-null|wrapper|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|stats-writer-null|0|0|0|15,64,15,1,0|",
        "flint-sound-use-array-null|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|use-array-null|0|0|0|15,64,15,1,0|",
        "flint-sound-break-array-null|wrapper|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|break-array-null|0|0|0|15,64,15,1,0|",
        "flint-damage63|direct|15,64,15|1|259|1|63|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-break64|direct|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-damage-overflow|direct|15,64,15|1|259|1|2147483647|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-zero-count|direct|15,64,15|1|259|0|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-zero-count-break|direct|15,64,15|1|259|0|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-negative-count-break|direct|15,64,15|1|259|-2|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-two-count-break|direct|15,64,15|1|259|2|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-negative-damage|direct|15,64,15|1|259|1|-2|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-wrapper-break64|wrapper|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-ob-break64|ob|15,64,15|1|259|1|64|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-direct-nondamageable-stack|direct|15,64,15|1|4|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|none|0|0|0|15,64,15,1,0|",
        "flint-initial-item-random-world|direct|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|true|none|0|0|0|15,64,15,1,0|",
        "flint-sound-target-stone|wrapper|15,64,15|1|259|1|0|5|1|2|false|false|false|false|false|false|false|false|false|false|false|false|false|target-stone|0|0|0|15,64,15,1,0|"
    };
    static final class Profile {
        final Map<String,Object> values=new LinkedHashMap<>();
        Profile(String text) {
            String[] parts=text.split("\\|",-1);
            PlayerDamageWakeObservation.need(parts.length==KEYS.length,"declared Flint operands");
            for(int i=0;i<KEYS.length;i++) {
                String k=KEYS[i],v=parts[i];Object value;
                if(k.equals("mode")||k.equals("operation")||k.equals("sound_action"))value=v;
                else if(k.equals("xyz"))value=FireObservation.Profile.numbers(v);
                else if(k.equals("cells")||k.equals("heights")) {
                    List<Object> rows=new ArrayList<>();if(!v.isEmpty())for(String row:v.split(";"))rows.add(FireObservation.Profile.numbers(row));value=rows;
                }else if(k.equals("remote")||k.equals("item_random_world")||k.startsWith("null_"))value=Boolean.valueOf(v);
                else value=Integer.valueOf(v);
                values.put(k,value);
            }
        }
        String text(String k){return (String)values.get(k);}
        int n(String k){return (Integer)values.get(k);}
        boolean flag(String k){return (Boolean)values.get(k);}
        int xyz(int i){return (Integer)((List<?>)values.get("xyz")).get(i);}
        FireObservation.Profile base() {
            List<String> fields=new ArrayList<>();
            for(String k:FireObservation.KEYS) {
                Object v=values.get(k);
                if(k.equals("rain")||k.equals("random_player")||k.startsWith("null_"))v=false;
                else if(k.equals("source_id"))v=1;
                else if(k.equals("entry_id"))v=51;
                else if(k.equals("random_seed"))v=n("world_seed");
                else if(k.equals("added_error_at"))v=0;
                else if(k.equals("g"))v=17;
                else if(k.equals("cooldown"))v=6001;
                else if(k.equals("prequeue_delay"))v=-1;
                else if(k.equals("biome"))v="i";
                fields.add(encode(v));
            }
            return new FireObservation.Profile(String.join("|",fields));
        }
    }
    static String encode(Object value) {
        if(value instanceof List){List<String> parts=new ArrayList<>();for(Object v:(List<?>)value)parts.add(encode(v));
            return String.join(!((List<?>)value).isEmpty()&&((List<?>)value).get(0) instanceof List?";":",",parts);}
        return String.valueOf(value);
    }
    static Profile profile;
    static iz stack;
    static dc player;
    static os controller;
    static gm receiver;
    static Random preparedItemRandom;
    static List<Integer> shifted() {
        int x=profile.xyz(0),y=profile.xyz(1),z=profile.xyz(2),face=profile.n("face");
        if(face==0)y--;if(face==1)y++;if(face==2)z--;if(face==3)z++;if(face==4)x--;if(face==5)x++;
        return List.of(x,y,z);
    }
    static Object cell(int x,int y,int z) {
        if(x< -32000000||x>=32000000||z< -32000000||z>32000000||y<0||y>=128)return List.of(0,0);
        for(lm c:FireObservation.chunks)if(c.j==(x>>4)&&c.k==(z>>4))return List.of(c.b[((x&15)<<11)|((z&15)<<7)|y]&255,c.e.a(x&15,y,z&15));
        return null;
    }
    static Object target(){List<Integer> t=shifted();return cell(t.get(0),t.get(1),t.get(2));}
    static Object clicked(){return cell(profile.xyz(0),profile.xyz(1),profile.xyz(2));}
    static Object stack()throws Exception{return stack==null?null:FireObservation.map("id",stack.c,"count",stack.a,"damage",FireObservation.get(stack,iz.class,"d"),"animation",stack.b);}
    static Object stats()throws Exception{return FireObservation.map("total",PlayerDamageWakeObservation.statMap("a"),"unsent",PlayerDamageWakeObservation.statMap("b"),
        "dirty",FireObservation.get(PlayerDamageWakeObservation.writer,xi.class,"c"),"writer_null",PlayerDamageWakeObservation.app.I==null);}
    static Object controller()throws Exception {
        if(controller==null)return null;Map<String,Object> out=new LinkedHashMap<>();
        for(String key:new String[]{"c","d","e","f","g","h","i"}){Object v=FireObservation.get(controller,os.class,key);out.put(key,v instanceof Float?FireObservation.f((Float)v):v);}return out;
    }
    static Object fields()throws Exception {
        Map<String,Object> out=new LinkedHashMap<>();
        for(String key:new String[]{"bf","bg","a","bh","bi","bj","bk","bl"}) {
            Object v=FireObservation.get(receiver,gm.class,key);if(v instanceof gm)v=((gm)v).bf;out.put(key,v);
        }return out;
    }
    static Random itemRandom()throws Exception{return (Random)FireObservation.get(null,gm.class,"b");}
    static Object state()throws Exception {
        fd w=FireObservation.world;Map<Integer,lm> cache=(Map<Integer,lm>)FireObservation.get(FireObservation.preparedProvider,ok.class,"e");
        PlayerDamageWakeObservation.need(cache.size()==FireObservation.chunks.size(),"complete prepared Flint Chunk footprint");
        List<Object> planes=new ArrayList<>(),regions=new ArrayList<>(),order=new ArrayList<>();
        for(lm c:FireObservation.chunks){PlayerDamageWakeObservation.need(cache.get(yy.a(c.j,c.k))==c,"same actual Chunk receiver");
            planes.add(FireObservation.map("xyz",List.of(c.j,c.k),"planes",FireObservation.map("blocks",PlayerDamageWakeObservation.hash(c.b),"metadata",PlayerDamageWakeObservation.hash(c.e.a),
                "sky",PlayerDamageWakeObservation.hash(c.f.a),"block_light",PlayerDamageWakeObservation.hash(c.g.a),"height",PlayerDamageWakeObservation.hash(c.h))));}
        for(Object value:(List<?>)FireObservation.get(w,fd.class,"C")){st r=(st)value;regions.add(FireObservation.map("kind",r.a.name(),"bounds",List.of(r.b,r.c,r.d,r.e,r.f,r.g)));}
        for(Object value:(java.util.Set<?>)FireObservation.get(w,fd.class,"P")){yy r=(yy)value;order.add(List.of(r.a,r.b));}
        return FireObservation.map("clicked",clicked(),"target_xyz",shifted(),"target",target(),"world_random",FireObservation.random(w.r),"math_random",FireObservation.random(FireObservation.math),
            "item_random",FireObservation.random(itemRandom()),"prepared_item_random",FireObservation.random(preparedItemRandom),"item_random_alias_world",itemRandom()==w.r,
            "time",String.format(Locale.ROOT,"%016x",w.x.f()),"world_o",w.o,"sky_seen",lm.a,"remote",w.B,
            "provider_null",FireObservation.get(w,fd.class,"v")==null,"world_access_null",FireObservation.get(w,fd.class,"u")==null,
            "item_table_null",gm.c==null,"use_stats_null",jl.E==null,"break_stats_null",jl.F==null,
            "scheduled",UprightPlantsWakeObservation.scheduled(),"entity_class",FireObservation.classField(sn.class,"a",false),"lighting",FireObservation.map("depth",fd.A,"regions",regions),
            "fire",uu.as==null?null:uu.as.bn,"flint_fields",fields(),"stack",stack(),"stats",stats(),"controller",controller(),
            "entities",FireObservation.entities(),"players",FireObservation.players(),"entity_count",w.b.size(),"player_count",w.d.size(),
            "weather",FireObservation.map("rain_previous",FireObservation.f((Float)FireObservation.get(w,fd.class,"i")),"rain_current",FireObservation.f((Float)FireObservation.get(w,fd.class,"j")),
                "thunder_previous",FireObservation.f((Float)FireObservation.get(w,fd.class,"k")),"thunder_current",FireObservation.f((Float)FireObservation.get(w,fd.class,"l")),
                "rain_time",w.x.p(),"thunder_time",w.x.n(),"raining",w.x.o(),"thundering",w.x.m()),
            "random_work",FireObservation.map("g",FireObservation.get(w,fd.class,"g"),"cooldown",FireObservation.get(w,fd.class,"Q"),"candidates",order),"chunk_planes",planes);
    }
    static final class Observer implements pm {
        final List<Object> events=new ArrayList<>();int cells,ranges,sounds;
        Map<String,Object> event(String kind)throws Exception {
            PlayerDamageWakeObservation.need(events.size()<256,"bounded actual Flint callbacks");
            return FireObservation.map("kind",kind,"clicked",clicked(),"target_xyz",shifted(),"target",target(),"world_o",FireObservation.world.o,
                "world_random",FireObservation.random(FireObservation.world.r),"math_random",FireObservation.random(FireObservation.math),"item_random",FireObservation.random(itemRandom()),
                "prepared_item_random",FireObservation.random(preparedItemRandom),"item_random_alias_world",itemRandom()==FireObservation.world.r,
                "stack",stack(),"fire",uu.as==null?null:uu.as.bn,"stats",stats(),"item_table_null",gm.c==null,"use_stats_null",jl.E==null,"break_stats_null",jl.F==null,
                "stack_trace",UprightPlantsWakeObservation.stack(Thread.currentThread().getStackTrace()));
        }
        void interrupt(String kind,int count){if(count==profile.n(kind+"_error_at"))throw new IllegalStateException("Flint and Steel "+kind+" observer interruption");}
        public void a(int x,int y,int z){try{Map<String,Object> e=event("cell");e.put("xyz",List.of(x,y,z));e.put("cell",cell(x,y,z));events.add(e);}catch(Exception e){throw new AssertionError(e);}interrupt("cell",++cells);}
        public void b(int x,int y,int z,int a,int b,int c){try{Map<String,Object> e=event("range");e.put("xyz",List.of(x,y,z,a,b,c));events.add(e);}catch(Exception e){throw new AssertionError(e);}interrupt("range",++ranges);}
        public void a(String name,double x,double y,double z,float v,float p){try{Map<String,Object> e=event("sound");e.put("name",name);e.put("position",List.of(FireObservation.d(x),FireObservation.d(y),FireObservation.d(z)));e.put("volume",FireObservation.f(v));e.put("pitch",FireObservation.f(p));events.add(e);soundAction();}catch(Exception e){throw new AssertionError(e);}interrupt("sound",++sounds);}
        public void e(){try{events.add(event("sky"));}catch(Exception e){throw new AssertionError(e);}}
        public void a(sn value){throw new AssertionError("undeclared Entity publication");}
        public void b(sn value){throw new AssertionError("undeclared Entity removal");}
        public void a(String n,double x,double y,double z,double a,double b,double c){throw new AssertionError("undeclared particle");}
        public void a(String n,int x,int y,int z){throw new AssertionError("undeclared record");}
        public void a(int x,int y,int z,ow value){throw new AssertionError("undeclared TileEntity");}
        public void a(gs p,int a,int b,int c,int d,int e){throw new AssertionError("undeclared level event");}
    }
    static void staticField(Class<?> owner,String name,Object value)throws Exception {
        Class<?> type=Class.forName("sun.misc.Unsafe");Object unsafe=FireObservation.get(null,type,"theUnsafe");Field field=owner.getDeclaredField(name);
        Object base=type.getMethod("staticFieldBase",Field.class).invoke(unsafe,field);long offset=(Long)type.getMethod("staticFieldOffset",Field.class).invoke(unsafe,field);
        type.getMethod("putObject",Object.class,long.class,Object.class).invoke(unsafe,base,offset,value);
    }
    static void soundAction()throws Exception {
        switch(profile.text("sound_action")) {
            case "none":break;
            case "fire-null":staticField(uu.class,"as",null);break;
            case "fire-id-zero":PlayerDamageWakeObservation.field(uu.class,"bn").setInt(uu.as,0);break;
            case "item-random-world":FireObservation.put(null,gm.class,"b",FireObservation.world.r);break;
            case "stack-stick":stack.c=280;break;
            case "stats-writer-null":PlayerDamageWakeObservation.app.I=null;break;
            case "use-array-null":staticField(jl.class,"E",null);break;
            case "break-array-null":staticField(jl.class,"F",null);break;
            case "target-stone":List<Integer> target=shifted();FireObservation.world.f(target.get(0),target.get(1),target.get(2),1);break;
            default:throw new AssertionError("declared sound action");
        }
    }
    static void run(Profile p,int index)throws Exception {
        profile=p;controller=null;Map<String,Object> classes=(Map<String,Object>)FireObservation.setup(p.base(),index);
        Map<String,Object> preparedStats=(Map<String,Object>)stats();preparedStats.remove("writer_null");classes.put("stats",preparedStats);
        fd w=FireObservation.world;kq app=PlayerDamageWakeObservation.app;
        player=new dc(app,w,app.k,0);app.f=w;app.h=player;app.i=player;player.e(8.5,64,8.5);
        PlayerDamageWakeObservation.need(w.b(player),"genuine local Player publication");if(p.text("operation").equals("ob"))controller=new os(app);
        w.r.setSeed(p.n("world_seed"));FireObservation.math.setSeed(0);preparedItemRandom.setSeed(p.n("item_seed"));
        w.B=p.flag("remote");stack=p.flag("null_stack")?null:new iz(p.n("stack_id"),p.n("count"),p.n("damage"));if(stack!=null)stack.b=p.n("animation");
        receiver=gm.c[259];PlayerDamageWakeObservation.need(receiver.getClass()==ye.class,"actual registered YE receiver");
        yq fire=uu.as;int fireId=fire.bn;Object items=gm.c,use=jl.E,broken=jl.F,provider=FireObservation.get(w,fd.class,"v");xi writer=app.I;
        if(p.flag("item_random_world"))FireObservation.put(null,gm.class,"b",w.r);
        if(p.flag("null_item_random"))FireObservation.put(null,gm.class,"b",null);
        if(p.flag("null_fire"))staticField(uu.class,"as",null);
        if(p.flag("null_provider"))FireObservation.put(w,fd.class,"v",null);
        if(p.flag("null_stats_writer"))app.I=null;
        if(p.flag("null_use_stats"))staticField(jl.class,"E",null);
        if(p.flag("null_break_stats"))staticField(jl.class,"F",null);
        if(p.flag("null_item_table"))staticField(gm.class,"c",null);
        Observer observer=new Observer();w.a(observer);Object accesses=FireObservation.get(w,fd.class,"u");if(p.flag("null_access"))FireObservation.put(w,fd.class,"u",null);
        Object before=state(),result=null;String exception="NONE",message=null;List<Object> trace=new ArrayList<>();
        fd argument=p.flag("null_world")?null:w;gs actor=p.flag("null_player")?null:player;
        try {
            int x=p.xyz(0),y=p.xyz(1),z=p.xyz(2),face=p.n("face");
            switch(p.text("operation")) {
                case "direct":result=receiver.a(stack,actor,argument,x,y,z,face);break;
                case "wrapper":result=stack.a(actor,argument,x,y,z,face);break;
                case "ob":result=controller.a(actor,argument,stack,x,y,z,face);break;
                default:throw new AssertionError("declared Flint entry");
            }
        }catch(RuntimeException failure){exception=failure.getClass().getName().replace('.','/');message=failure.getMessage();trace=UprightPlantsWakeObservation.stack(failure.getStackTrace());}
        Object after=state();
        System.out.println("FLINT_AND_STEEL "+ConstructorJson.json(FireObservation.map("mode",p.text("mode"),"prepared",p.values,"prepared_class",classes,
            "receiver",FireObservation.map("class",receiver.getClass().getName(),"id",receiver.bf),"before",before,"after",after,"result",result,
            "exception",exception,"exception_message",message,"exception_stack",trace,"events",observer.events)));
        staticField(uu.class,"as",fire);PlayerDamageWakeObservation.field(uu.class,"bn").setInt(fire,fireId);staticField(gm.class,"c",items);staticField(jl.class,"E",use);staticField(jl.class,"F",broken);
        FireObservation.put(null,gm.class,"b",preparedItemRandom);FireObservation.put(w,fd.class,"v",provider);FireObservation.put(w,fd.class,"u",accesses);app.I=writer;
    }
    @SuppressWarnings("removal")
    public static void main(String[] args)throws Exception {
        PlayerDamageWakeObservation.need(args.length==3,"archive, isolated directory, class names");Locale.setDefault(Locale.ROOT);
        PlayerDamageWakeObservation.archive=Path.of(args[0]).toRealPath();PlayerDamageWakeObservation.directory=Path.of(args[1]).toRealPath();
        System.setSecurityManager(new PlayerDamageWakeObservation.Guard());PlayerDamageWakeObservation.bind(Path.of(args[2]));
        kq app=new kq(null,null,null,854,480,false,null);PlayerDamageWakeObservation.app=app;
        try {
            app.k=new gr("FireComponent","-");FireObservation.put(app,net.minecraft.client.Minecraft.class,"Z",PlayerDamageWakeObservation.directory.toFile());
            PlayerDamageWakeObservation.writer=new xi(app.k,PlayerDamageWakeObservation.directory.toFile());app.I=PlayerDamageWakeObservation.writer;
            ep.f.a(new kh(app));PlayerDamageWakeObservation.workers();
            FireObservation.math=(Random)FireObservation.get(null,Class.forName("java.lang.Math$RandomNumberGeneratorHolder"),"randomNumberGenerator");PlayerDamageWakeObservation.shared=FireObservation.math;
            preparedItemRandom=itemRandom();
            for(int i=0;i<DATA.length;i++)run(new Profile(DATA[i]),i);
            PlayerDamageWakeObservation.need(PlayerDamageWakeObservation.network==0&&PlayerDamageWakeObservation.nativeLinks==0&&PlayerDamageWakeObservation.rejectedWrites==0&&PlayerDamageWakeObservation.execs==0,"effect guards");
        }finally{if(PlayerDamageWakeObservation.writer!=null)PlayerDamageWakeObservation.workers();app.J=false;
            for(Thread t:Thread.getAllStackTraces().keySet())if(t.getName().equals("Timer hack thread")){t.interrupt();t.join(2000);PlayerDamageWakeObservation.need(!t.isAlive(),"Timer worker retirement");}}
        System.out.println("FLINT_AND_STEEL_COMPLETE cases="+DATA.length+" network=0 native=0 rejected_writes=0 exec=0 GUI=0");
    }
}
