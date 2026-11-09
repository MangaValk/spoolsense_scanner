#ifndef LANE_PINS_H
#define LANE_PINS_H

#include "BoardPins.h"

// Multi-reader ("lane") support: one ESP32 driving several readers on one SPI
// bus, e.g. one reader per BoxTurtle/AFC lane. Lane 1 is the reader on the
// regular NFC pins (BoardPins.h + the #201 overrides). Each further lane adds
// its own chip select (NSS/SS) and, on the PN5180, its own BUSY line; RST,
// SCK, MOSI and MISO are shared.
//
// NFC_MAX_LANES caps how many lanes the config page offers on this board. The
// default tables below are only suggestions for lanes 2..N; every lane pin can
// be changed on the config page and is validated against the board's rules.
// -1 = no default (the user must pick a pin).
//
// Boards that share their SPI bus with the TFT (BOARD_SHARED_SPI) and the
// ESP32-C3 stay single-lane.

#if defined(BOARD_S3_ZERO)
  // Lanes 2-6 on the side headers (1, 2, 10-13) and the bottom pads (14-17).
  // 10-12 are the PN5180 GPIO/IRQ/AUX inputs, which the firmware never reads.
  #define NFC_MAX_LANES 6
  #define NFC_LANE_DEFAULT_NSS  { -1, 10, 12, 1, 14, 16 }
  #define NFC_LANE_DEFAULT_BUSY { -1, 11, 13, 2, 15, 17 }
#elif defined(BOARD_S3_DEVKITC)
  #define NFC_MAX_LANES 6
  #define NFC_LANE_DEFAULT_NSS  { -1, -1, -1, -1, -1, -1 }
  #define NFC_LANE_DEFAULT_BUSY { -1, -1, -1, -1, -1, -1 }
#elif defined(BOARD_SHARED_SPI) || defined(BOARD_ESP32_C3) || defined(BOARD_ESP32_C5) || \
      defined(BOARD_ESP32_C6) || defined(BOARD_SEEED_XIAO_ESP32_C6) || defined(BOARD_ESP32_S3)
  #define NFC_MAX_LANES 1
  #define NFC_LANE_DEFAULT_NSS  { -1 }
  #define NFC_LANE_DEFAULT_BUSY { -1 }
#else
  // Classic ESP32 (WROOM-32, D1 mini ESP32). BUSY on the input-only GPIO34-39
  // where possible. Lanes 2-3 use the same chip selects as the 4-lane PN532
  // wiring from #337 (18, 32).
  #define NFC_MAX_LANES 6
  #define NFC_LANE_DEFAULT_NSS  { -1, 18, 32, 21, 22, 16 }
  #define NFC_LANE_DEFAULT_BUSY { -1, 34, 35, 36, 39, 17 }
#endif

#endif // LANE_PINS_H
