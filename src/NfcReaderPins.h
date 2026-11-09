#ifndef NFC_READER_PINS_H
#define NFC_READER_PINS_H

#include <cstdint>
#include "ConfigurationManager.h"

// The pin set one reader connection drives. PN532 and RC522 use the same
// slots (SS = nss) and ignore busy.
//
// A connection built with the default constructor reads these from
// ConfigurationManager at begin(), as before (#201). A connection built
// with an explicit set uses that set instead, which is how a second reader
// on the same SPI bus gets its own chip select and BUSY line.
struct NfcReaderPins {
    uint8_t rst = 0;
    uint8_t nss = 0;
    uint8_t busy = 0;
    uint8_t sck = 0;
    uint8_t mosi = 0;
    uint8_t miso = 0;

    // Effective pins from the config: NVS override or board default.
    static NfcReaderPins fromConfig() {
        auto& cfg = ConfigurationManager::getInstance();
        NfcReaderPins p;
        p.rst  = cfg.getNfcPin(NfcPinId::Rst);
        p.nss  = cfg.getNfcPin(NfcPinId::Nss);
        p.busy = cfg.getNfcPin(NfcPinId::Busy);
        p.sck  = cfg.getNfcPin(NfcPinId::Sck);
        p.mosi = cfg.getNfcPin(NfcPinId::Mosi);
        p.miso = cfg.getNfcPin(NfcPinId::Miso);
        return p;
    }
};

#endif // NFC_READER_PINS_H
