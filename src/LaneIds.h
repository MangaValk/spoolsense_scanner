#ifndef LANE_IDS_H
#define LANE_IDS_H

#include <cstddef>
#include <cstdint>

// MQTT device IDs when one ESP32 drives several readers ("lanes").
//
// Each lane is published as its own virtual scanner, so the middleware and
// Home Assistant treat it exactly like a separate SpoolSense: per-lane
// spoolsense/<id>/tag/state, spoolsense/<id>/cmd/#, and one HA device each.
//
//   lane count 1:  <base>                  (stock ID, e.g. "ecb008")
//   lane count N:  <base>-L1 ... <base>-LN  (same scheme as #337)
//
// Hardware-free so the native tests cover it.
namespace LaneIds {

// Longest ID: 6 hex chars + "-L" + up to 3 digits + NUL
static constexpr size_t MAX_LEN = 12;

// Writes lane's device ID (lane is 0-based). Returns false if it didn't fit.
bool format(const char* base, uint8_t lane, uint8_t laneCount, char* out, size_t outLen);

// The 0-based lane an ID belongs to, or -1 if it isn't one of ours.
// id need not be NUL-terminated after idLen (it can point into a topic).
int parse(const char* base, uint8_t laneCount, const char* id, size_t idLen);

}  // namespace LaneIds

#endif  // LANE_IDS_H
