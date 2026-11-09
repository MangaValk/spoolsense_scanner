#include "LaneConfig.h"

#include <cstdio>

namespace LaneConfig {

namespace {

// Record one claimed pin; fails if another lane or function already has it.
bool claim(int16_t pin, const char* what, uint8_t lane,
           int16_t* usedPin, const char** usedWhat, uint8_t* usedLane, size_t& used,
           char* err, size_t errLen) {
    for (size_t i = 0; i < used; i++) {
        if (usedPin[i] == pin) {
            if (usedLane[i] == 0) {
                snprintf(err, errLen, "Lane %u %s GPIO %d is already used by the %s",
                         (unsigned)lane, what, (int)pin, usedWhat[i]);
            } else {
                snprintf(err, errLen, "Lane %u %s GPIO %d is already used by lane %u %s",
                         (unsigned)lane, what, (int)pin, (unsigned)usedLane[i], usedWhat[i]);
            }
            return false;
        }
    }
    usedPin[used] = pin;
    usedWhat[used] = what;
    usedLane[used] = lane;
    used++;
    return true;
}

}  // namespace

bool validate(uint8_t laneCount, uint8_t maxLanes,
              const int16_t* nss, const int16_t* busy, bool useBusy,
              const ReservedPin* reserved, size_t reservedCount,
              PinCapFn cap, char* err, size_t errLen) {
    if (err && errLen) err[0] = '\0';
    if (laneCount < 1 || laneCount > maxLanes) {
        snprintf(err, errLen, "Lane count must be between 1 and %u on this board", (unsigned)maxLanes);
        return false;
    }

    // Every claimed pin: reserved functions first (lane 0), then each lane.
    static constexpr size_t MAX_CLAIMS = 96;
    int16_t usedPin[MAX_CLAIMS];
    const char* usedWhat[MAX_CLAIMS];
    uint8_t usedLane[MAX_CLAIMS];
    size_t used = 0;
    for (size_t i = 0; i < reservedCount && used < MAX_CLAIMS; i++) {
        if (reserved[i].pin < 0) continue;
        usedPin[used] = reserved[i].pin;
        usedWhat[used] = reserved[i].what;
        usedLane[used] = 0;
        used++;
    }

    for (uint8_t i = 1; i < laneCount; i++) {
        const uint8_t lane = i + 1;
        if (nss[i] < 0 || nss[i] >= PIN_DEFAULT) {
            snprintf(err, errLen, "Lane %u needs an NSS/SS pin", (unsigned)lane);
            return false;
        }
        if (cap((uint8_t)nss[i]) != PinCap::InOut) {
            snprintf(err, errLen, "Lane %u NSS GPIO %d can't drive a chip select on this board",
                     (unsigned)lane, (int)nss[i]);
            return false;
        }
        if (used >= MAX_CLAIMS ||
            !claim(nss[i], "NSS", lane, usedPin, usedWhat, usedLane, used, err, errLen)) {
            return false;
        }

        if (!useBusy) continue;
        if (busy[i] < 0 || busy[i] >= PIN_DEFAULT) {
            snprintf(err, errLen, "Lane %u needs a BUSY pin", (unsigned)lane);
            return false;
        }
        if (cap((uint8_t)busy[i]) == PinCap::Unusable) {
            snprintf(err, errLen, "Lane %u BUSY GPIO %d isn't usable on this board",
                     (unsigned)lane, (int)busy[i]);
            return false;
        }
        if (used >= MAX_CLAIMS ||
            !claim(busy[i], "BUSY", lane, usedPin, usedWhat, usedLane, used, err, errLen)) {
            return false;
        }
    }
    return true;
}

}  // namespace LaneConfig
