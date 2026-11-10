# Fork changes: multi-reader lanes

This branch (`multilane`) adds support for **several readers on one ESP32**, one per "lane" (for example one per
BoxTurtle/AFC lane). Each lane appears to the middleware and Home Assistant as its own virtual scanner.

- **Base:** `upstream/dev` at `f9c6b34` (v1.11.3).
- **Sync tag:** `sync/upstream-f9c6b34`.
- **With `lane_count = 1` (the default):** the firmware behaves like stock. It has the same device ID, MQTT topics,
  discovery payloads and `/api/status`.

## Commits (one per logical step, each builds all envs and passes the native tests)

| Commit | Branch | What |
|---|---|---|
| `8e784ba` | `ml/01-reader-pins` | `NfcReaderPins`. Each reader connection (PN5180, PN532, RC522) can take an explicit pin set. |
| `876eaa7` | `ml/02-nfc-lane-state` | NFCManager's per-reader state is moved into `NFCLaneState` (a mechanical rename, no behaviour change). |
| `ab36f3b` | `ml/03-lane-config` | `lane_count` and per-lane NSS/BUSY: NVS, validation, `/api/config`, config page. |
| `a083a54` | `ml/04-lane-scanning` | One scan task walks the lanes; one RF field at a time; the lane goes into messages and writes. |
| `8152bdc` | `ml/05-lane-mqtt` | `<id>-L<n>` virtual scanners: MQTT, HA discovery per lane, command routing. |
| `5a8e721` | `ml/06-web-lanes` | `/api/lanes`, the per-lane reader test, the lanes list on the config page. |
| merge | `fix/usb-serial-tx-timeout` | The boot stall fix, sent upstream as SpoolSense/spoolsense_scanner#342. |

## New files (fork only, never conflict)

- **`include/LanePins.h`:** the per-board `NFC_MAX_LANES` and default lane pins.
- **`src/NfcReaderPins.h`:** the pin set of one reader, `fromConfig()` and `forLane()`.
- **`src/LaneConfig.{h,cpp}`:** lane pin validation (hardware-free).
- **`src/LaneIds.{h,cpp}`:** formats `<id>` / `<id>-L<n>` and parses a topic back to its lane (hardware-free).
- **`src/NFCManagerLanes.cpp`:**
  - `allocateLanes`, `beginExtraLanes`, `advanceScanLane`
  - `laneOfTag`, `testLane`, `isLaneOnline`
- **`src/HomeAssistantManagerLanes.cpp`:**
  - `getLaneDeviceId`, `selectLane`, `laneOfTopic`
  - `clearRemovedLanes` (NVS `lane_pub`)
- **`src/WebLanes.cpp`:** `/api/lanes`, `/api/lanes/test`, `serializeLanes`.
- **`test/native/test_lane_config.cpp`** and **`test/native/test_lane_ids.cpp`**.

## Touched upstream code

When a merge conflicts in one of these places, re-apply the "why" on top of the new upstream code.

| Upstream file | Function / area | Why touched |
|---|---|---|
| `src/HardwareNFCConnection{,PN532,RC522}.{h,cpp}` | constructors, `begin()` pin read | Optional explicit pin set (`NfcReaderPins`). `begin()` reads `explicitPins_` or the config. |
| `src/HardwareNFCConnection.{h,cpp}`, `HardwareNFCConnectionRC522.{h,cpp}` | `rfOff()` (new override) | Field off between lanes. |
| `src/NFCConnectionI.h` | `virtual void rfOff() {}` (appended) | Optional; the default does nothing. |
| `src/NFCManager.h` | per-tag members → `struct NFCLaneState` | Per-reader state. `lane0_`, `lanes_`, `laneCount_`, `onlineLanes_`, `scan_` added at the end of the class. |
| `src/NFCManager.h` | getter signatures (`getCurrentSpoolState`, `getLast*Data`, `*GenericTagSpoolInfo`, `getNfcReaderInfo`, `getDiagConnection`, `writeSpoolmanDataToTag`) | Optional `uint8_t lane = 0` parameter. |
| `src/NFCManager.h` | `resetWriteState()` | Drains every lane's queue. |
| `src/NFCManager.cpp` | every per-tag member reference (392, mechanical) | `X` → `scan_->X` on the scan task, `lanes_[n].X` from other tasks. |
| `src/NFCManager.cpp` | `begin()` | `allocateLanes()` first, `beginExtraLanes()` last. |
| `src/NFCManager.cpp` | `scanLoop()` | `advanceScanLane()` at the top of each cycle; the delay is spread across lanes. |
| `src/NFCManager.cpp` | `prepareRF()` | Cold start every turn when more than one lane has a reader. |
| `src/NFCManager.cpp` | `enqueueWrite()`, `requestCurrentSpool()`, `isWriteQueueEmpty()` | The request's lane / every lane. |
| `src/NFCManager.cpp` | every `AppMessage msg` in send functions | `msg.lane = scanLaneIndex()`. |
| `src/NFCWriteTypes.h` | `NFCWriteRequest` | `uint8_t lane` appended. |
| `src/ApplicationManager.h` | `AppMessage` | `uint8_t lane` appended. `msgLane_`, `laneHAState_`, `haStateJson()`, `cacheHAState()` at the end of the class. |
| `src/ApplicationManager.cpp` | `handleMessage()` | `msgLane_ = laneOfMessage(msg)` (new static helper above it). |
| `src/ApplicationManager.cpp` | `handleSpoolDetected/SpoolUpdated/SpoolmanSynced/HAWriteTag/HAUpdateRemaining` | NFC getters get `msgLane_`; `req.lane = msgLane_` before `enqueueWrite`. |
| `src/ApplicationManager.cpp` | `handleTagRemoved()` | Lane's cached tag/state; `clearGenericTagSpoolInfo(msgLane_)`. |
| `src/ApplicationManager.cpp` | 3× `lastHAStateJson_` store sites | `cacheHAState(json)`. |
| `src/ApplicationManager.cpp` | `publishToHA()` | Topic uses `getLaneDeviceId(msgLane_)`. |
| `src/DeductionManager.cpp` | `applyOpenPrintTag()`, `applyOpenTag3D()` | `laneOfTag(uid)`: read and write through the reader that holds that tag. |
| `src/HomeAssistantManager.h` | `deviceId_` size | Holds `<id>-L<n>`. `baseId_`, `lane_`, `laneDiscoveryUid_` and helpers at the end. |
| `src/HomeAssistantManager.cpp` | `begin()`, `reconnect()`, `publishAvailability()` | `baseId_` for client ID, LWT and availability (equal to `deviceId_` with one reader). |
| `src/HomeAssistantManager.cpp` | `taskLoop()` connect block | Discovery, subscribe and tag state per lane; `clearRemovedLanes()`. |
| `src/HomeAssistantManager.cpp` | `taskLoop()` publish queue | `selectLane(laneOfTopic(req.topic))` around the tag/state mirror block. |
| `src/HomeAssistantManager.cpp` | `publishDiscovery()` | `avty_t` and device name via `%s` (with one reader: `~/availability`, "SpoolSense Scanner"). |
| `src/HomeAssistantManager.cpp` | `publishCurrentTagState()` | Getters with `lane_`. |
| `src/HomeAssistantManager.cpp` | `mqttCallback()`, `handleCommand()` | Lane from the topic, `msg.lane = lane_`, back to lane 0 after. |
| `src/ConfigurationManager.h` | `ConfigUpdate` | `lane_count`, `lane_nss[8]`, `lane_busy[8]` appended. Getters, `validateLanes()`, members at the end. |
| `src/ConfigurationManager.cpp` | after `sanitizeNfcPinSet()` | `lanePinCap()`, `checkLanes()` (new statics). |
| `src/ConfigurationManager.cpp` | `loadFromNVS()`, `saveToNVS()`, `getCurrentConfig()` | NVS keys `lane_count`, `lane_nss`, `lane_busy`. |
| `src/ConfigurationManager.cpp` | after `getNfcPin()` | Lane getters. |
| `src/WebServerManager.h` | private | `handleApiLanes`, `handleApiLaneTest`, `serializeLanes`. |
| `src/WebServerManager.cpp` | `begin()` routes | `/api/lanes`, `/api/lanes/test`. |
| `src/WebServerManager.cpp` | `handleApiGetConfig()` / `handleApiPostConfig()` | `lane_count`, `lane_max`, `lanes[]`. POST validates (400 + reason) and keeps the current lanes when a client sends none. |
| `src/WebServerManager.cpp` | `handleApiStatus()` | `serializeLanes(doc)` (no-op with one reader). |
| `src/ConfigHTML.h` | NFC pins section, script | "Multiple Readers / Lanes" section, `initLanes`, `laneBody`, `loadLaneStatus`, `testLane`. |
| `src/main.cpp` | `setup()` | `Serial.setTxTimeoutMs(0)` (#342). |
| `test/native/Makefile` | all, test, clean, rules | `test_lane_config`, `test_lane_ids`. |

**Line endings:** upstream stores `src/ApplicationManager.h`, `src/NFCConnectionI.h` and `src/NFCWriteTypes.h` with
CRLF. Keep them CRLF (commit with `core.autocrlf=false` if needed), or the diff covers the whole file.

**New NVS keys** (namespace `spoolsense`): `lane_count` (u8), `lane_nss` (8 bytes), `lane_busy` (8 bytes) and
`lane_pub` (u8, the lane count last published to MQTT). They need a matching spoolsense-installer issue before an
upstream PR.

## Merge checklist (syncing with upstream)

1. Check what upstream changed since the last sync:
   - `git fetch upstream`
   - `git log --oneline sync/upstream-<last>..upstream/dev`
   - read the CHANGELOG for NFCManager, ApplicationManager, HomeAssistantManager, ConfigurationManager and the MQTT topics
2. Rebuild the stack on the new base, so each ml/* branch stays one clean commit for the PRs:
   - `git rebase upstream/dev ml/01-reader-pins`, then each next ml/* onto the one before
   - resolve conflicts with the table above
   - `git grep -n "scan_->\|lanes_\[" src/NFCManager.cpp` must still cover all per-tag state, including anything new upstream added
3. Recreate `multilane` from `ml/06-web-lanes` plus any not-yet-merged fix branches.
4. Build every env (`pio run`, from PowerShell with `PYTHONUTF8=1`) and run `make -C test/native test`.
5. Check the middleware contract: if `tag/state` or `cmd/...` changed, `LaneIds` and the HA lane code must follow.
6. On the bench:
   - `lane_count=1` must give the same boot log, discovery topics and payload sizes as stock
   - 2 lanes must scan, deduct and write to the right tag
7. Tag `sync/upstream-<sha>` and update the base line at the top of this file.
