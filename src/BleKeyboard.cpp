#include "BleKeyboard.h"
#include <Arduino.h>

namespace {
// Keyboard application collection: modifiers, reserved byte, six key usages.
// The key array Logical Maximum is 231 (0xE7), which needs the 2-byte form
// (0x26 E7 00): a 1-byte 0xE7 would be read as signed -25. Usage Maximum is
// unsigned, so 0x29 E7 is fine. Report ID is carried by the GATT
// report-reference descriptor, not payload.
uint8_t reportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xE7, 0x00,
    0x05, 0x07, 0x19, 0x00, 0x29, 0xE7, 0x81, 0x00, 0xC0};
constexpr uint8_t kMaxUsage = 0xE7;
}

void MacroKeyboard::begin(const char* name, uint16_t keystrokeMs) {
  keystrokeMs_ = keystrokeMs;
  NimBLEDevice::init(name);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  server_ = NimBLEDevice::createServer();
  server_->setCallbacks(this, false);
  server_->advertiseOnDisconnect(true);
  hid_ = new NimBLEHIDDevice(server_);
  input_ = hid_->getInputReport(1);
  input_->setCallbacks(this);
  hid_->getOutputReport(1);  // Keyboard LED report; no physical LEDs to update.
  hid_->setManufacturer("Stick-S3 Macro");
  // Unassigned USB vendor/product IDs for this private prototype.
  hid_->setPnp(2, 0, 0, 0x0100);
  hid_->setHidInfo(0, 0x02);
  hid_->setReportMap(reportMap, sizeof(reportMap));
  uint8_t released[8]{};
  input_->setValue(released, sizeof(released));
  hid_->setBatteryLevel(100);
  server_->start();
  auto* advertising = NimBLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid_->getHidService()->getUUID());
  advertising->enableScanResponse(true);
  advertising->setName(name);
  advertising->setPreferredParams(12, 24);
  advertising->start();
  Serial.printf("BLE advertising: %s\n", name);
}

bool MacroKeyboard::connected() const {
  return connection_.load() != BLE_HS_CONN_HANDLE_NONE;
}

bool MacroKeyboard::ready() const {
  return connected() && encrypted_.load() && subscribed_.load();
}

void MacroKeyboard::resetPending() {
  phase_ = Phase::Idle;
  abortHold_ = false;
  mods_ = usage_ = 0;
  baselinePending_ = true;
}

void MacroKeyboard::syncGeneration() {
  const uint32_t generation = generation_.load();
  if (generation != seenGeneration_) {
    resetPending();
    seenGeneration_ = generation;
  }
}

bool MacroKeyboard::send(uint8_t mods, uint8_t usage) {
  syncGeneration();
  if (!ready() || phase_ != Phase::Idle) return false;
  if ((mods == 0 && usage == 0) || usage > kMaxUsage) return false;
  mods_ = mods;
  usage_ = usage;
  phase_ = Phase::PressPending;
  return true;
}

bool MacroKeyboard::sendReport(uint8_t mods, uint8_t usage) {
  if (!ready()) return false;
  const uint16_t handle = connection_.load();
  uint8_t report[8] = {mods, 0, usage, 0, 0, 0, 0, 0};
  input_->setValue(report, sizeof(report));
  return input_->notify(report, sizeof(report), handle);
}

void MacroKeyboard::releaseAll() {
  syncGeneration();
  if (phase_ == Phase::PressPending) {
    phase_ = Phase::Idle;  // never sent; nothing to release
  } else if (phase_ == Phase::Held) {
    if (sendReport(0, 0)) {
      phase_ = Phase::Gap;
      phaseAt_ = millis();
    } else {
      abortHold_ = true;  // update() retries without waiting out the hold
    }
  }
}

void MacroKeyboard::update(uint32_t now) {
  syncGeneration();
  if (!ready()) {
    resetPending();
    return;
  }
  // Establish a neutral report after every connection/subscription transition.
  if (baselinePending_) {
    if (!sendReport(0, 0)) return;
    baselinePending_ = false;
  }
  switch (phase_) {
    case Phase::Idle:
      break;
    case Phase::PressPending:
      if (sendReport(mods_, usage_)) {
        phase_ = Phase::Held;
        phaseAt_ = now;
      }
      break;
    case Phase::Held:
      if (!abortHold_ && uint32_t(now - phaseAt_) < keystrokeMs_) break;
      // Retry a failed release on later loops; never advance until it lands.
      if (sendReport(0, 0)) {
        abortHold_ = false;
        phase_ = Phase::Gap;
        phaseAt_ = now;
      }
      break;
    case Phase::Gap:
      if (uint32_t(now - phaseAt_) >= keystrokeMs_) phase_ = Phase::Idle;
      break;
  }
}

void MacroKeyboard::setBatteryLevel(uint8_t level) {
  if (hid_) hid_->setBatteryLevel(level > 100 ? 100 : level, ready());
}

void MacroKeyboard::clearBonds() {
  // Intended for startup, before accepting button gestures.
  const uint16_t handle = connection_.load();
  if (ready()) sendReport(0, 0);
  subscribed_.store(false);
  encrypted_.store(false);
  generation_.fetch_add(1);
  resetPending();
  if (server_ && handle != BLE_HS_CONN_HANDLE_NONE) server_->disconnect(handle);
  NimBLEDevice::deleteAllBonds();
}

void MacroKeyboard::onConnect(NimBLEServer* server, NimBLEConnInfo& info) {
  if (connected()) {
    server->disconnect(info.getConnHandle());
    return;
  }
  encrypted_.store(false);
  subscribed_.store(false);
  connection_.store(info.getConnHandle());
  generation_.fetch_add(1);
  server->updateConnParams(info.getConnHandle(), 12, 24, 0, 400);
  NimBLEDevice::startSecurity(info.getConnHandle());
}

void MacroKeyboard::onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) {
  if (connection_.load() != info.getConnHandle()) return;
  connection_.store(BLE_HS_CONN_HANDLE_NONE);
  encrypted_.store(false);
  subscribed_.store(false);
  generation_.fetch_add(1);
}

void MacroKeyboard::onAuthenticationComplete(NimBLEConnInfo& info) {
  if (connection_.load() != info.getConnHandle()) return;
  encrypted_.store(info.isEncrypted());
  if (!info.isEncrypted()) {
    generation_.fetch_add(1);
    server_->disconnect(info.getConnHandle());
  }
}

void MacroKeyboard::onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& info,
                                uint16_t value) {
  if (connection_.load() != info.getConnHandle()) return;
  subscribed_.store((value & 1) != 0);
  generation_.fetch_add(1);
}
