#ifndef MC_TEST_BLOCK_STATIC_INDEX_CASES_H
#define MC_TEST_BLOCK_STATIC_INDEX_CASES_H

/* Independent ordered uu/na field literals, not generated from the decoder. */
static const char *const block_static_names[] = {
    "u",  "v",  "w",  "x",  "y",  "z",  "A",  "B",  "C",  "D",  "E",  "F",  "G",  "H",  "I",  "J",
    "K",  "L",  "M",  "N",  "O",  "P",  "Q",  "R",  "S",  "T",  "U",  "V",  "W",  "X",  "Y",  "Z",
    "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj", "ak", "al", "am", "an", "ao", "ap",
    "aq", "ar", "as", "at", "au", "av", "aw", "ax", "ay", "az", "aA", "aB", "aC", "aD", "aE", "aF",
    "aG", "aH", "aI", "aJ", "aK", "aL", "aM", "aN", "aO", "aP", "aQ", "aR", "aS", "aT", "aU", "aV",
    "aW", "aX", "aY", "aZ", "ba", "bb", "bc", "bd", "be", "bf", "bg", "bh", "bi", "bj", "bk", "bl"};

static const char *const block_static_invalid[] = {"",
                                                   "a",
                                                   "b",
                                                   "t",
                                                   "0",
                                                   "_",
                                                   "m",
                                                   "n",
                                                   "p",
                                                   "AA",
                                                   "Ba",
                                                   "bA",
                                                   "bm",
                                                   "bz",
                                                   "ca",
                                                   "a0",
                                                   "a_",
                                                   "a[",
                                                   "a`",
                                                   "a{",
                                                   "b`",
                                                   "uX",
                                                   "aaX",
                                                   "blX",
                                                   "aaa",
                                                   "missing",
                                                   "\x80",
                                                   "\xff",
                                                   "a\x80",
                                                   "b\xff",
                                                   ("\x80"
                                                    "a"),
                                                   "\xc3\xa9",
                                                   " a",
                                                   "a ",
                                                   "u\n"};

#endif
