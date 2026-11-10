// HomeAssistantManager: multi-reader ("lane") support.
//
// When one ESP32 drives several readers, each lane is published as its own
// virtual scanner: spoolsense/<id>-L<n>/tag/state, .../cmd/#, and one Home
// Assistant device per lane ("SpoolSense Lane N"). The middleware and HA treat
// each lane exactly like a separate SpoolSense. With one reader the ID stays
// the stock <id> and nothing here changes what is published.
//
// The MQTT connection, client ID and availability/LWT stay the ESP's own
// (baseId_); everything per tag goes through deviceId_, which selectLane()
// points at one lane at a time on the MQTT task.

#include "HomeAssistantManager.h"

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <Preferences.h>
#include "NFCManager.h"
#endif

#include <cstring>

void HomeAssistantManager::getLaneDeviceId(uint8_t lane, char* buf, size_t bufSize) {
    char base[7];
    getDeviceId(base, sizeof(base));
    if (!LaneIds::format(base, lane, laneCount(), buf, bufSize)) {
        LaneIds::format(base, 0, laneCount(), buf, bufSize);  // unknown lane: the first one
    }
}

uint8_t HomeAssistantManager::laneCount() {
#ifndef NATIVE_TEST
    return NFCManager::getInstance().getLaneCount();
#else
    return 1;
#endif
}

void HomeAssistantManager::selectLane(uint8_t lane) {
    if (lane >= laneCount()) lane = 0;
    lane_ = lane;
    if (!LaneIds::format(baseId_, lane_, laneCount(), deviceId_, sizeof(deviceId_))) {
        strncpy(deviceId_, baseId_, sizeof(deviceId_) - 1);
        deviceId_[sizeof(deviceId_) - 1] = '\0';
    }
}

char* HomeAssistantManager::discoveryUid() {
    // Lane 0 keeps upstream's lastDiscoveryUid_
    return (lane_ == 0) ? lastDiscoveryUid_ : laneDiscoveryUid_[lane_ < LANE_SLOTS ? lane_ : 0];
}

uint8_t HomeAssistantManager::laneOfTopic(const char* topic) const {
    static const char prefix[] = "spoolsense/";
    const size_t prefixLen = sizeof(prefix) - 1;
    if (topic == nullptr || strncmp(topic, prefix, prefixLen) != 0) return 0;
    const char* idEnd = strchr(topic + prefixLen, '/');
    if (idEnd == nullptr) return 0;
    int lane = LaneIds::parse(baseId_, laneCount(), topic + prefixLen, (size_t)(idEnd - topic - prefixLen));
    return (lane > 0) ? (uint8_t)lane : 0;
}

// When the lane count changes, the virtual scanners that no longer exist
// would linger in Home Assistant (retained discovery) and the middleware
// (retained tag/state). Clear exactly those, once, using the count published
// last time (NVS "lane_pub"). Going from 1 to several lanes clears the stock
// <id> device; going back clears the -L<n> devices.
void HomeAssistantManager::clearRemovedLanes() {
#ifndef NATIVE_TEST
    Preferences prefs;
    if (!prefs.begin("spoolsense", false)) return;
    const uint8_t prev = prefs.getUChar("lane_pub", 1);
    const uint8_t now = laneCount();
    if (prev == now) {
        prefs.end();
        return;
    }

    // Object IDs of every entity publishDiscovery() creates
    static const char* const ENTITIES[][2] = {
        {"sensor", "spool"}, {"sensor", "printer_warning"},
        {"number", "set_remaining_weight"}, {"number", "set_initial_weight"},
        {"number", "set_spoolman_id"}, {"select", "set_material_type"},
        {"text", "set_manufacturer"},
    };
    auto clearId = [&](const char* id) {
        char topic[128];
        for (const auto& e : ENTITIES) {
            snprintf(topic, sizeof(topic), "homeassistant/%s/spoolsense_%s/%s/config", e[0], id, e[1]);
            mqttClient.publish(topic, "", true);
        }
        snprintf(topic, sizeof(topic), "spoolsense/%s/tag/state", id);
        mqttClient.publish(topic, "", true);
        snprintf(topic, sizeof(topic), "spoolsense/%s/tag/attributes", id);
        mqttClient.publish(topic, "", true);
        Serial.printf("HomeAssistantManager: cleared retained topics of removed scanner %s\n", id);
    };

    char id[LaneIds::MAX_LEN];
    if (prev <= 1 && now > 1) {
        clearId(baseId_);                                   // stock device -> lanes
    } else if (prev > 1) {
        for (uint8_t lane = (now > 1) ? now : 0; lane < prev; lane++) {
            if (LaneIds::format(baseId_, lane, prev, id, sizeof(id))) clearId(id);  // removed lanes
        }
    }
    prefs.putUChar("lane_pub", now);
    prefs.end();
#endif
}
