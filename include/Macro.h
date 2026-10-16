#pragma once
#include <stdint.h>

// Shared macro data model. config.yaml is compiled into these structures by
// scripts/generate_config.py; firmware never parses key names or text.

// One HID keystroke, or a pause when usage == 0 && delayMs > 0.
struct Stroke {
  uint8_t mods;      // HID modifier bits (0x01 CTRL, 0x02 SHIFT, 0x04 ALT, 0x08 GUI)
  uint8_t usage;     // HID keyboard usage, 0 for modifier-only chords or pauses
  uint16_t delayMs;  // pause length; only meaningful when usage == 0
};

// A slot's action. count == 0 means unassigned.
struct Macro {
  const char* label;  // short uppercase display label, "" when unassigned
  const Stroke* strokes;
  uint16_t count;
};

enum Button : uint8_t { BTN_MAIN = 0, BTN_SECONDARY = 1, BUTTON_COUNT = 2 };
enum GestureKind : uint8_t { G_SINGLE = 0, G_DOUBLE = 1, G_HOLD = 2, GESTURE_COUNT = 3 };

struct Profile {
  const char* name;  // 1-12 printable ASCII characters
  Macro slots[BUTTON_COUNT][GESTURE_COUNT];
};

inline bool isPause(const Stroke& s) { return s.usage == 0 && s.delayMs > 0; }
