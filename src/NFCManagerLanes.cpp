// NFCManager: multi-reader ("lane") support.
//
// One ESP32 can drive several readers of the same type on one SPI bus, one
// per lane (e.g. one per BoxTurtle/AFC lane). Lane 0 is the reader on the
// regular NFC pins and is started by NFCManager::begin() exactly as before;
// this file adds lanes 1..n-1 and the round-robin between them.
//
// - Only one RF field is on at a time: the scan task finishes a lane, turns
//   its field off, then moves on. On the PN5180 the shared RST pulse at the
//   start of every turn resets all readers anyway, so each turn starts cold.
// - A lane whose reader doesn't answer at boot is marked offline and never
//   polled, so a missing or unplugged reader can't slow the others down.
// - With one lane (the default) none of this runs and nothing is allocated.

#include "NFCManager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "ConfigurationManager.h"
#include "HardwareNFCConnection.h"
#include "HardwareNFCConnectionPN532.h"
#include "HardwareNFCConnectionRC522.h"
#include "LogBuffer.h"
#include "NfcReaderPins.h"
#include <new>
#endif

#include <cstring>

namespace {

#ifndef NATIVE_TEST
// A connection of the configured reader type on one lane's pins.
NFCConnectionI* createLaneConnection(const NfcReaderPins& pins) {
    const char* reader = ConfigurationManager::getInstance().getNfcReader();
    if (strcmp(reader, "pn532") == 0) return new (std::nothrow) HardwareNFCConnectionPN532(pins);
    if (strcmp(reader, "rc522") == 0) return new (std::nothrow) HardwareNFCConnectionRC522(pins);
    return new (std::nothrow) HardwareNFCConnection(pins);
}

// begin() succeeding isn't proof of a reader: with nothing on the lane's chip
// select, SPI reads come back as all-zeros or all-ones. Require a sane
// firmware version.
bool readerAnswers(NFCConnectionI* conn) {
    ReaderDiagnostics d{};
    if (!conn->getDiagnosticSnapshot(d) || !d.initialized) return false;
    const bool allOnes = d.fw_major == 0xFF && d.fw_minor == 0xFF;
    const bool allZero = d.fw_major == 0 && d.fw_minor == 0;
    return !allOnes && !allZero;
}
#endif

}  // namespace

bool NFCManager::allocateLanes() {
#ifndef NATIVE_TEST
    const uint8_t count = ConfigurationManager::getInstance().getLaneCount();
    if (count <= 1 || lanes_ != &lane0_) {
        return true;  // one reader, or already allocated by an earlier begin()
    }
    NFCLaneState* lanes = new (std::nothrow) NFCLaneState[count];
    if (lanes == nullptr) {
        Serial.printf("NFCManager: not enough memory for %u lanes, using 1\n", (unsigned)count);
        return true;  // fall back to the single built-in lane
    }
    // A connection injected before begin() (PN532/RC522 from main.cpp) is lane 0's
    lanes[0].connection_ = lane0_.connection_;
    lanes[0].ownsConnection_ = lane0_.ownsConnection_;
    lane0_.connection_ = nullptr;
    lane0_.ownsConnection_ = false;
    lanes_ = lanes;
    laneCount_ = count;
    scan_ = &lanes_[0];
    Serial.printf("NFCManager: %u lanes, %u bytes of lane state each\n",
                  (unsigned)count, (unsigned)sizeof(NFCLaneState));
#endif
    return true;
}

bool NFCManager::beginExtraLanes() {
#ifndef NATIVE_TEST
    auto& log = LogBuffer::getInstance();
    for (uint8_t i = 1; i < laneCount_; i++) {
        NFCLaneState& lane = lanes_[i];
        const NfcReaderPins pins = NfcReaderPins::forLane(i);
        lane.online = false;
        memset(&lane.currentSpool, 0, sizeof(lane.currentSpool));
        lane.writeQueue = xQueueCreate(8, sizeof(NFCWriteRequest));
        lane.connection_ = createLaneConnection(pins);
        lane.ownsConnection_ = lane.connection_ != nullptr;
        if (lane.writeQueue == nullptr || lane.connection_ == nullptr) {
            log.logPrintf("NFCManager: lane %u: out of memory\n", (unsigned)(i + 1));
            continue;
        }
        const unsigned long t0 = millis();
        lane.online = lane.connection_->begin() && readerAnswers(lane.connection_);
        // A PN5180 that doesn't answer trips the driver's bus-wedged latch,
        // which is shared by every PN5180 on the bus. Release it so a missing
        // reader can't put the working lanes into fail-fast mode.
        PN5180::clearBusWedged();
        char info[24] = "no reader";
        if (lane.online) {
            lane.connection_->getReaderInfo(info, sizeof(info));
        } else {
            // Offline lanes hold no driver objects
            delete lane.connection_;
            lane.connection_ = nullptr;
            lane.ownsConnection_ = false;
        }
        log.logPrintf("NFCManager: lane %u (NSS=%u BUSY=%u): %s (%lu ms)\n",
                      (unsigned)(i + 1), (unsigned)pins.nss, (unsigned)pins.busy, info,
                      millis() - t0);
    }
    onlineLanes_ = 0;
    for (uint8_t i = 0; i < laneCount_; i++) {
        if (isLaneOnline(i)) onlineLanes_++;
    }
    if (laneCount_ > 1) {
        log.logPrintf("NFCManager: %u of %u lanes have a reader\n", (unsigned)onlineLanes_, (unsigned)laneCount_);
    }
#endif
    return true;
}

bool NFCManager::isLaneOnline(uint8_t lane) const {
    return lane < laneCount_ && lanes_[lane].online && lanes_[lane].connection_ != nullptr;
}

int NFCManager::laneOfTag(const char* uid) {
    if (uid == nullptr || uid[0] == '\0') return -1;
    if (laneCount_ <= 1) {
        return 0;  // one reader: everything belongs to it, as before
    }
    int found = -1;
    if (tagMutex && xSemaphoreTake(tagMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (uint8_t i = 0; i < laneCount_ && found < 0; i++) {
            const CurrentSpoolState& s = lanes_[i].currentSpool;
            if (s.present && strncmp(s.spool_id, uid, sizeof(s.spool_id)) == 0) found = i;
        }
        xSemaphoreGive(tagMutex);
    }
    return found;
}

void NFCManager::advanceScanLane() {
    if (onlineLanes_ <= 1) return;  // one working reader: exactly the single-reader behaviour
    // Finish the lane we just polled: field off, so only one is ever on
    if (scan_->connection_ && scan_->online) {
        scan_->connection_->rfOff();
    }
    uint8_t idx = scanLaneIndex();
    for (uint8_t step = 0; step < laneCount_; step++) {
        idx = (uint8_t)((idx + 1) % laneCount_);
        if (isLaneOnline(idx)) {
            scan_ = &lanes_[idx];
            return;
        }
    }
    // No other lane online: stay on the current one (lane 0 always is)
}
