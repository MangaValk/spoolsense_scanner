// WebServerManager: multi-reader ("lane") endpoints.
//
//   GET  /api/lanes              every lane: device ID, pins, reader, current tag
//   POST /api/lanes/test?lane=N  probe lane N's reader (1-based), for checking
//                                each cable while wiring
//
// /api/status also gets lane_count and lanes[] — but only when more than one
// reader is configured, so a single-reader scanner's status is unchanged.

#include "WebServerManager.h"
#include "ConfigurationManager.h"
#include "ConversionUtils.h"
#include "HomeAssistantManager.h"
#include "LaneIds.h"
#include "NFCManager.h"
#include "NFCTypes.h"

#include <ArduinoJson.h>

namespace {

void addLane(JsonObject l, uint8_t lane) {
    auto& nfc = NFCManager::getInstance();
    auto& cfg = ConfigurationManager::getInstance();
    char id[LaneIds::MAX_LEN];
    HomeAssistantManager::getLaneDeviceId(lane, id, sizeof(id));
    l["lane"] = lane + 1;
    l["device_id"] = id;
    l["nss"] = cfg.getLaneNss(lane);
    l["busy"] = cfg.getLaneBusy(lane);
    l["online"] = nfc.isLaneOnline(lane);
    char reader[24] = "no reader";
    if (nfc.isLaneOnline(lane)) nfc.getNfcReaderInfo(reader, sizeof(reader), lane);
    l["reader"] = reader;
    CurrentSpoolState state;
    if (nfc.getCurrentSpoolState(state, lane) && state.present) {
        l["present"] = true;
        l["uid"] = state.spool_id;
        l["tag_format"] = tagKindToMqttFormat(state.kind);  // as in the lane's MQTT tag/state
        l["tag_data_valid"] = state.tag_data_valid;
    } else {
        l["present"] = false;
    }
}

}  // namespace

void WebServerManager::serializeLanes(JsonDocument& doc) {
    const uint8_t count = NFCManager::getInstance().getLaneCount();
    if (count <= 1) return;
    doc["lane_count"] = count;
    JsonArray lanes = doc["lanes"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) addLane(lanes.add<JsonObject>(), i);
}

void WebServerManager::handleApiLanes() {
    JsonDocument doc;
    const uint8_t count = NFCManager::getInstance().getLaneCount();
    doc["lane_count"] = count;
    doc["lane_max"] = ConfigurationManager::getInstance().getMaxLanes();
    JsonArray lanes = doc["lanes"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) addLane(lanes.add<JsonObject>(), i);
    String body;
    serializeJson(doc, body);
    _server.send(200, "application/json", body);
}

void WebServerManager::handleApiLaneTest() {
    const long laneArg = _server.hasArg("lane") ? _server.arg("lane").toInt() : 0;
    const uint8_t count = NFCManager::getInstance().getLaneCount();
    if (laneArg < 1 || laneArg > count) {
        sendError(400, "lane must be between 1 and the lane count");
        return;
    }
    LaneTestResult r;
    if (!NFCManager::getInstance().testLane((uint8_t)(laneArg - 1), r)) {
        sendError(503, r.reader);  // e.g. "scanner busy"
        return;
    }
    JsonDocument doc;
    doc["lane"] = laneArg;
    doc["nss"] = r.nss;
    doc["busy"] = r.busy;
    doc["answers"] = r.answers;
    doc["reader"] = r.reader;
    doc["was_online"] = r.online;
    doc["restart_needed"] = !r.online && r.answers;  // found now, but not scanned until restart
    doc["ms"] = r.ms;
    String body;
    serializeJson(doc, body);
    _server.send(200, "application/json", body);
}
