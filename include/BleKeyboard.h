#pragma once

#include <atomic>
#include <cstdint>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// Single-stroke BLE HID keyboard transport.
//
// send() accepts one stroke (modifiers + usage) when ready() and idle(). On a
// later update() it notifies the press report, holds it keystrokeMs, sends an
// explicit all-zero release (retried on later updates until it succeeds), then
// waits a further keystrokeMs gap before idle() is true again. Pacing between
// strokes and delays is the caller's job (MacroPlayer); there is no queue and
// no queue-age expiry. A stroke accepted before a reconnect/resubscribe is
// never sent after it (generation check).
//
// Call begin/update/send/releaseAll/setBatteryLevel/clearBonds from the Arduino
// loop task. BLE callbacks only publish atomic connection state.
class MacroKeyboard : private NimBLEServerCallbacks,
                      private NimBLECharacteristicCallbacks {
 public:
  void begin(const char* name, uint16_t keystrokeMs);
  void update(uint32_t now);
  // usage 0 with mods != 0 is a modifier-only chord. Rejects mods == 0 &&
  // usage == 0, usage > 0xE7, not ready, or not idle.
  bool send(uint8_t mods, uint8_t usage);
  // True when no stroke is pending, held, or in its release gap.
  bool idle() const { return phase_ == Phase::Idle; }
  // Abort: drop an unsent stroke; release a held key as soon as possible.
  void releaseAll();
  bool connected() const;
  bool ready() const;
  void setBatteryLevel(uint8_t level);
  void clearBonds();

 private:
  enum class Phase : uint8_t { Idle, PressPending, Held, Gap };

  void onConnect(NimBLEServer*, NimBLEConnInfo&) override;
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override;
  void onAuthenticationComplete(NimBLEConnInfo&) override;
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) override;
  bool sendReport(uint8_t mods, uint8_t usage);
  void syncGeneration();
  void resetPending();

  NimBLEServer* server_ = nullptr;
  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* input_ = nullptr;
  std::atomic<uint16_t> connection_{BLE_HS_CONN_HANDLE_NONE};
  std::atomic<bool> encrypted_{false};
  std::atomic<bool> subscribed_{false};
  std::atomic<uint32_t> generation_{0};
  uint32_t seenGeneration_ = 0;
  Phase phase_ = Phase::Idle;
  uint8_t mods_ = 0;
  uint8_t usage_ = 0;
  bool abortHold_ = false;
  bool baselinePending_ = true;
  uint16_t keystrokeMs_ = 15;
  uint32_t phaseAt_ = 0;
};
