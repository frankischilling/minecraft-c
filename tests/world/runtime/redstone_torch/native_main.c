#include "emit_current.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--utility-controls-only") == 0) {
        if (!db_emit_utility_controls())
            return 1;
        return puts("PASS: 3 SHA256 utility vectors") < 0 ? 1 : 0;
    }
    int result = 1;
    FILE *profiles = NULL;
    db_fixture *fixture = calloc(1, sizeof(*fixture));
    db_snapshot *before = calloc(1, sizeof(*before));
    db_snapshot *after = calloc(1, sizeof(*after));
    db_outcome *outcome = calloc(1, sizeof(*outcome));
    db_profile *profile = calloc(1, sizeof(*profile));
    bool closed = false, close_attempted = false;
    size_t count = 0, expected = 0;
#define MAIN_REQUIRE(value)                                                                        \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "DB_MAIN line%d: %s\n", __LINE__, #value);                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
#ifdef _WIN32
    MAIN_REQUIRE(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
    MAIN_REQUIRE(argc == 5 && fixture != NULL && before != NULL && after != NULL &&
                 outcome != NULL && profile != NULL);
    MAIN_REQUIRE(db_emit_utility_controls());
    if (strcmp(argv[4], "lifecycle") == 0)
        expected = 70;
    else if (strcmp(argv[4], "powered") == 0)
        expected = 32;
    else if (strcmp(argv[4], "burnout") == 0)
        expected = 78;
    else if (strcmp(argv[4], "aging") == 0)
        expected = 23;
    MAIN_REQUIRE(expected != 0);
#ifdef _MSC_VER
    MAIN_REQUIRE(fopen_s(&profiles, argv[3], "rb") == 0);
#else
    profiles = fopen(argv[3], "rb");
#endif
    MAIN_REQUIRE(profiles != NULL && fflush(stdout) == 0);
    MAIN_REQUIRE(db_fixture_open(fixture, argv[1], argv[2], strcmp(argv[4], "aging") == 0));
    MAIN_REQUIRE(fixture->opened_complete && db_emit_worlds(stdout, fixture));
    for (;;) {
        bool eof = false;
        MAIN_REQUIRE(db_profile_read(profiles, profile, &eof));
        if (eof)
            break;
        MAIN_REQUIRE(count < expected && db_prepare(fixture, profile));
        MAIN_REQUIRE(db_snapshot_take(fixture, before));
        const bool invoked = db_invoke(fixture, profile, outcome);
        MAIN_REQUIRE(db_snapshot_take(fixture, after));
        MAIN_REQUIRE(
            db_emit_row(stdout, argv[4], count, profile, fixture, before, after, outcome, invoked));
        ++count;
        db_snapshot_retire(after);
        db_snapshot_retire(before);
        db_outcome_retire(outcome);
        MAIN_REQUIRE(invoked);
    }
    MAIN_REQUIRE(count == expected);
    const int profile_close = fclose(profiles);
    profiles = NULL;
    MAIN_REQUIRE(profile_close == 0);
    close_attempted = true;
    MAIN_REQUIRE(db_fixture_close(fixture));
    closed = true;
    MAIN_REQUIRE(db_emit_finish(stdout, argv[4], count, fixture) && fflush(stdout) == 0 &&
                 !ferror(stdout));
    result = 0;
cleanup:
    if (profiles != NULL)
        (void)fclose(profiles);
    db_snapshot_retire(after);
    db_snapshot_retire(before);
    db_outcome_retire(outcome);
    if (fixture != NULL && fixture->opened_complete && !close_attempted) {
        close_attempted = true;
        if (!db_fixture_close(fixture))
            result = 1;
        else
            closed = true;
    }
    /* Partial open/failed retirement retains its owner graph until process
     * exit. Do not free borrowed callback contexts or fabricate cleanup. */
    if (closed || (fixture != NULL && fixture->files == NULL))
        free(fixture);
    free(profile);
    free(outcome);
    free(after);
    free(before);
    return result;
#undef MAIN_REQUIRE
}
