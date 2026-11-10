// Lane device-ID tests. These IDs are the MQTT contract with the middleware
// (spoolsense/<id>/tag/state, spoolsense/<id>/cmd/...), so a single lane must
// keep the stock ID and every command topic must map back to exactly one lane.
#include "LaneIds.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
                              else { printf("  PASS %s\n", msg); } } while (0)

static int parseStr(const char* base, uint8_t count, const char* id) {
    return LaneIds::parse(base, count, id, strlen(id));
}

int main() {
    printf("=== Lane ID tests ===\n");
    char id[LaneIds::MAX_LEN];

    CHECK(LaneIds::format("ecb008", 0, 1, id, sizeof(id)) && strcmp(id, "ecb008") == 0,
          "one lane keeps the stock device ID");
    CHECK(LaneIds::format("ecb008", 0, 5, id, sizeof(id)) && strcmp(id, "ecb008-L1") == 0,
          "multi-lane: lane 1 is <id>-L1");
    CHECK(LaneIds::format("ecb008", 4, 5, id, sizeof(id)) && strcmp(id, "ecb008-L5") == 0,
          "multi-lane: lane 5 is <id>-L5");
    CHECK(!LaneIds::format("ecb008", 5, 5, id, sizeof(id)) && id[0] == '\0',
          "lane beyond the count is refused");
    CHECK(!LaneIds::format("ecb008", 0, 2, id, 8) && id[0] == '\0',
          "too-small buffer is refused, never truncated");
    CHECK(LaneIds::format("ecb008", 0, 0, id, sizeof(id)) && strcmp(id, "ecb008") == 0,
          "lane count 0 is treated as 1");

    CHECK(parseStr("ecb008", 1, "ecb008") == 0, "stock ID maps to lane 0");
    CHECK(parseStr("ecb008", 1, "ecb008-L1") == -1, "single lane ignores -L suffixes");
    CHECK(parseStr("ecb008", 5, "ecb008-L1") == 0, "-L1 maps to lane 0");
    CHECK(parseStr("ecb008", 5, "ecb008-L5") == 4, "-L5 maps to lane 4");
    CHECK(parseStr("ecb008", 5, "ecb008-L6") == -1, "lane above the count is rejected");
    CHECK(parseStr("ecb008", 5, "ecb008-L0") == -1, "-L0 is rejected");
    CHECK(parseStr("ecb008", 5, "ecb008-L01") == -1, "leading zero is rejected");
    CHECK(parseStr("ecb008", 5, "ecb008") == -1, "bare ID is not a lane in multi-lane mode");
    CHECK(parseStr("ecb008", 5, "ecb009-L1") == -1, "another scanner's ID is rejected");
    CHECK(parseStr("ecb008", 5, "ecb008-X1") == -1, "wrong suffix is rejected");
    CHECK(parseStr("ecb008", 12, "ecb008-L12") == 11, "two-digit lanes parse");

    // Parsing straight out of a topic: the ID is not NUL-terminated there
    const char* topic = "spoolsense/ecb008-L3/cmd/deduct/04A1B2";
    const char* idStart = topic + strlen("spoolsense/");
    const char* idEnd = strchr(idStart, '/');
    CHECK(LaneIds::parse("ecb008", 5, idStart, (size_t)(idEnd - idStart)) == 2,
          "ID parsed in place from a command topic");

    // Round trip for every lane
    bool roundTrip = true;
    for (uint8_t n = 1; n <= 8; n++) {
        for (uint8_t lane = 0; lane < n; lane++) {
            if (!LaneIds::format("ecb008", lane, n, id, sizeof(id)) ||
                parseStr("ecb008", n, id) != lane) {
                roundTrip = false;
            }
        }
    }
    CHECK(roundTrip, "format -> parse round-trips for 1..8 lanes");

    printf(failures ? "SOME TESTS FAILED (%d failures)\n" : "ALL PASSED (0 failures)\n", failures);
    return failures ? 1 : 0;
}
