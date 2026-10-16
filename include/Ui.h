#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include <string.h>
#include "Macro.h"

enum class LinkState : uint8_t { SEARCHING, HANDSHAKE, SECURE };
enum class Screen : uint8_t { HOME, PICKER, TRANSMIT };

// Plain description of what the display should show. main fills it; Ui draws it.
struct UiModel {
  Screen screen = Screen::HOME;
  LinkState link = LinkState::SEARCHING;
  int battery = -1;  // percent, -1 unknown
  bool charging = false;
  const char* deviceName = "";
  uint8_t profileIndex = 0, profileCount = 0;
  const Profile* profiles = nullptr;  // active = profiles[profileIndex]
  uint8_t highlighted = 0;            // picker
  const char* macroLabel = "";        // transmit
  uint16_t sent = 0, total = 0;       // transmit

  bool operator==(const UiModel& o) const {
    return screen == o.screen && link == o.link && battery == o.battery &&
           charging == o.charging && same(deviceName, o.deviceName) &&
           profileIndex == o.profileIndex && profileCount == o.profileCount &&
           profiles == o.profiles && highlighted == o.highlighted &&
           same(macroLabel, o.macroLabel) && sent == o.sent && total == o.total;
  }
  bool operator!=(const UiModel& o) const { return !(*this == o); }

 private:
  static bool same(const char* a, const char* b) {
    return a == b || (a && b && strcmp(a, b) == 0);
  }
};

class Ui {
 public:
  explicit Ui(M5GFX& display);
  void begin();                                        // create the 240x135 sprite
  void boot(const char* deviceName);                   // ~700 ms, blocking
  void render(const UiModel& m);                       // full redraw + push
  void bondReset(uint32_t remainingMs, bool cleared);  // A+B boot maintenance
  void fatal(const char* msg);                         // e.g. wrong board

 private:
  M5Canvas canvas_;
};
