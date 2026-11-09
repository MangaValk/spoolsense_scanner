// Lane (multi-reader) pin validation tests. LaneConfig::validate is the gate
// between the config page and NVS: a bad assignment must be refused with a
// readable reason, never saved half-applied.
#include "LaneConfig.h"
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
                              else { printf("  PASS %s\n", msg); } } while (0)

using LaneConfig::PinCap;
using LaneConfig::ReservedPin;

// Classic-ESP32-like rules: straps and flash unusable, 34-39 input-only.
static PinCap esp32Cap(uint8_t pin) {
    if (pin == 0 || pin == 2 || (pin >= 6 && pin <= 11) || pin == 12 || pin == 15 || pin > 39) {
        return PinCap::Unusable;
    }
    if (pin >= 34) return PinCap::InputOnly;
    return PinCap::InOut;
}

// The classic ESP32's shared NFC bus plus lane 1 (board defaults) and the LED.
static const ReservedPin RESERVED[] = {
    {13, "NFC RST"}, {25, "NFC SCK"}, {27, "NFC MOSI"}, {26, "NFC MISO"},
    {14, "lane 1 NSS"}, {33, "lane 1 BUSY"}, {4, "status LED"}, {-1, "unused TFT pin"},
};
static const size_t RESERVED_N = sizeof(RESERVED) / sizeof(RESERVED[0]);

static bool run(uint8_t count, const int16_t* nss, const int16_t* busy, bool useBusy,
                char* err, size_t errLen, uint8_t maxLanes = 6) {
    return LaneConfig::validate(count, maxLanes, nss, busy, useBusy, RESERVED, RESERVED_N,
                                esp32Cap, err, errLen);
}

int main() {
    printf("=== Lane config validation tests ===\n");
    char err[128];
    //                        lane:  1     2   3   4   5   6
    const int16_t goodNss[]  = {  -1,  18, 32, 21, 22, 16 };
    const int16_t goodBusy[] = {  -1,  34, 35, 36, 39, 17 };

    CHECK(run(1, goodNss, goodBusy, true, err, sizeof(err)),
          "one lane (stock) is always valid, lane pins ignored");
    CHECK(run(6, goodNss, goodBusy, true, err, sizeof(err)),
          "board defaults for six lanes are valid");
    CHECK(err[0] == '\0', "no error text on success");

    CHECK(!run(0, goodNss, goodBusy, true, err, sizeof(err)), "zero lanes refused");
    CHECK(!run(7, goodNss, goodBusy, true, err, sizeof(err)), "more lanes than the board allows refused");
    CHECK(strstr(err, "between 1 and 6") != nullptr, "lane count error names the board limit");

    {
        const int16_t nss[]  = { -1, 18, 18 };
        const int16_t busy[] = { -1, 34, 35 };
        CHECK(!run(3, nss, busy, true, err, sizeof(err)), "two lanes sharing an NSS refused");
        CHECK(strstr(err, "lane 2 NSS") != nullptr && strstr(err, "Lane 3") != nullptr,
              "duplicate error names both lanes");
    }
    {
        const int16_t nss[]  = { -1, 18 };
        const int16_t busy[] = { -1, 18 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "a lane's NSS and BUSY on the same pin refused");
    }
    {
        const int16_t nss[]  = { -1, 25 };
        const int16_t busy[] = { -1, 34 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "lane pin on the shared SCK refused");
        CHECK(strstr(err, "NFC SCK") != nullptr, "conflict error names the owner");
    }
    {
        const int16_t nss[]  = { -1, 4 };
        const int16_t busy[] = { -1, 34 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "lane pin on the status LED refused");
    }
    {
        const int16_t nss[]  = { -1, 14 };
        const int16_t busy[] = { -1, 34 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "lane 2 on lane 1's NSS refused");
    }
    {
        const int16_t nss[]  = { -1, 34 };
        const int16_t busy[] = { -1, 35 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "input-only GPIO refused as NSS");
        CHECK(strstr(err, "chip select") != nullptr, "input-only error explains why");
    }
    {
        const int16_t nss[]  = { -1, 18 };
        const int16_t busy[] = { -1, 39 };
        CHECK(run(2, nss, busy, true, err, sizeof(err)), "input-only GPIO accepted as BUSY");
    }
    {
        const int16_t nss[]  = { -1, 6 };
        const int16_t busy[] = { -1, 34 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "flash pin refused as NSS");
    }
    {
        const int16_t nss[]  = { -1, 18 };
        const int16_t busy[] = { -1, 12 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "strap pin refused as BUSY");
    }
    {
        const int16_t nss[]  = { -1, LaneConfig::PIN_DEFAULT };
        const int16_t busy[] = { -1, 34 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "lane without an NSS refused");
        CHECK(strstr(err, "needs an NSS") != nullptr, "missing-pin error is explicit");
    }
    {
        const int16_t nss[]  = { -1, 18 };
        const int16_t busy[] = { -1, -1 };
        CHECK(!run(2, nss, busy, true, err, sizeof(err)), "PN5180 lane without BUSY refused");
        CHECK(run(2, nss, busy, false, err, sizeof(err)), "PN532/RC522 lane needs no BUSY");
    }
    {
        // PN532/RC522: the BUSY values are ignored, even when they would collide.
        const int16_t nss[]  = { -1, 18, 32 };
        const int16_t busy[] = { -1, 18, 18 };
        CHECK(run(3, nss, busy, false, err, sizeof(err)), "BUSY ignored when the reader has none");
    }
    {
        const int16_t nss[]  = { -1, 18 };
        const int16_t busy[] = { -1, 34 };
        CHECK(!LaneConfig::validate(2, 1, nss, busy, true, RESERVED, RESERVED_N, esp32Cap, err, sizeof(err)),
              "board without multi-lane support refuses a second lane");
    }

    printf(failures ? "SOME TESTS FAILED (%d failures)\n" : "ALL PASSED (0 failures)\n", failures);
    return failures ? 1 : 0;
}
