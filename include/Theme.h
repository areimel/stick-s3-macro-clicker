#pragma once
#include <stdint.h>

// Display palette, mirroring the web flasher tokens (web/src/styles/theme.css).
// Colors are converted from 8-bit RGB to RGB565 at compile time.
namespace Theme {
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return uint16_t(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

constexpr uint16_t BG = rgb565(0x14, 0x13, 0x10);      // #141310 warm charcoal
constexpr uint16_t PANEL = rgb565(0x1c, 0x1b, 0x17);   // #1c1b17
constexpr uint16_t INK = rgb565(0xef, 0xeb, 0xe0);     // #efebe0 ivory
constexpr uint16_t DIM = rgb565(0xa3, 0x9e, 0x90);     // #a39e90
constexpr uint16_t LINE = rgb565(0x39, 0x37, 0x2f);    // #39372f hairline
constexpr uint16_t ACCENT = rgb565(0xff, 0x5b, 0x1a);  // #ff5b1a safety orange
constexpr uint16_t BLUE = rgb565(0x24, 0x38, 0xe8);    // #2438e8 (use sparingly)
constexpr uint16_t ALERT = rgb565(0xe0, 0x20, 0x18);   // fatal-error red

static_assert(BG == 0x1082, "BG RGB565 conversion");
static_assert(ACCENT == 0xFAC3, "ACCENT RGB565 conversion");
}  // namespace Theme
