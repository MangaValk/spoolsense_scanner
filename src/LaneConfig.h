#ifndef LANE_CONFIG_H
#define LANE_CONFIG_H

#include <cstddef>
#include <cstdint>

// Validation for multi-reader ("lane") pin assignments. Hardware-free, so the
// native tests can exercise it; ConfigurationManager supplies the board rules.
//
// Lane 1 is the reader on the regular NFC pins (#201). Every further lane adds
// its own chip select (NSS/SS) and, on the PN5180, its own BUSY line; RST, SCK,
// MOSI and MISO are shared by all lanes.
namespace LaneConfig {

// Sentinel for "use this board's default pin" (same value as LED_PIN_DEFAULT).
static constexpr uint8_t PIN_DEFAULT = 0xFF;

enum class PinCap : uint8_t {
    Unusable,   // strap, flash, USB, not routed on this board
    InputOnly,  // e.g. GPIO34-39 on the classic ESP32: fine for BUSY, not for NSS
    InOut,
};
using PinCapFn = PinCap (*)(uint8_t pin);

// A pin some other function already owns. `what` names it in error messages.
struct ReservedPin {
    int16_t pin;
    const char* what;
};

// Checks lanes 2..laneCount. nss[i] / busy[i] are the EFFECTIVE pins of lane
// i+1 (defaults already resolved), for i = 1..laneCount-1; index 0 is unused.
// useBusy is false for readers without a BUSY line (PN532, RC522).
// Returns true when the assignment is safe; otherwise writes a short,
// user-facing reason into err.
bool validate(uint8_t laneCount, uint8_t maxLanes,
              const int16_t* nss, const int16_t* busy, bool useBusy,
              const ReservedPin* reserved, size_t reservedCount,
              PinCapFn cap, char* err, size_t errLen);

}  // namespace LaneConfig

#endif  // LANE_CONFIG_H
