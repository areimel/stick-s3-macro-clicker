#include "Ui.h"
#include <Arduino.h>
#include "Theme.h"

namespace {
using namespace Theme;
constexpr int W = 240, H = 135;

// ---- helpers --------------------------------------------------------------
int textWidth(const char* s, int size, int spacing) {
  int n = int(strlen(s));
  return n ? n * (6 * size + spacing) - spacing : 0;
}

// Letter-spaced Font0 text; ASCII only.
void label(M5Canvas& c, int x, int y, const char* s, uint16_t color, int size = 1,
           int spacing = 1) {
  c.setFont(&fonts::Font0);
  c.setTextSize(size);
  c.setTextColor(color);
  for (; *s; ++s) {
    c.setCursor(x, y);
    c.write(uint8_t(*s));
    x += 6 * size + spacing;
  }
}

void labelRight(M5Canvas& c, int right, int y, const char* s, uint16_t color, int size = 1,
                int spacing = 1) {
  label(c, right - textWidth(s, size, spacing), y, s, color, size, spacing);
}

void labelCenter(M5Canvas& c, int cx, int y, const char* s, uint16_t color, int size = 1,
                 int spacing = 1) {
  label(c, cx - textWidth(s, size, spacing) / 2, y, s, color, size, spacing);
}

void upper(char* out, size_t cap, const char* in) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < cap; ++i) {
    char ch = in[i];
    out[i] = (ch >= 'a' && ch <= 'z') ? char(ch - 32) : ch;
  }
  out[i] = 0;
}

void square(M5Canvas& c, int x, int y, int s, uint16_t color) { c.fillRect(x, y, s, s, color); }

// L-shaped crosshair tick at a corner; dx/dy are +1/-1 pointing inward.
void tick(M5Canvas& c, int x, int y, int dx, int dy, uint16_t color) {
  c.drawFastHLine(dx > 0 ? x : x - 5, y, 6, color);
  c.drawFastVLine(x, dy > 0 ? y : y - 5, 6, color);
}

void cornerTicks(M5Canvas& c, int x0, int y0, int x1, int y1, uint16_t color) {
  tick(c, x0, y0, 1, 1, color);
  tick(c, x1, y0, -1, 1, color);
  tick(c, x0, y1, 1, -1, color);
  tick(c, x1, y1, -1, -1, color);
}

void drawBattery(M5Canvas& c, int right, int y, int pct, bool charging) {
  int x = right - 18;  // body 16x7 plus 2px nub, right aligned
  c.drawRect(x, y, 16, 7, DIM);
  c.fillRect(x + 16, y + 2, 2, 3, DIM);
  if (pct >= 0) {
    int w = (pct * 12 + 50) / 100;
    if (pct > 0 && w < 1) w = 1;
    c.fillRect(x + 2, y + 2, w, 3, charging ? ACCENT : (pct <= 15 ? ALERT : INK));
  }
  char buf[8];
  if (pct < 0) snprintf(buf, sizeof buf, "--");
  else snprintf(buf, sizeof buf, "%d%%", pct);
  labelRight(c, x - 4, y, buf, pct < 0 ? DIM : INK, 1, 0);
  if (charging) labelRight(c, x - 10 - textWidth(buf, 1, 0), y, "CHG", ACCENT, 1, 1);
}

void frameFooter(M5Canvas& c, const char* left, const char* right) {
  c.drawFastHLine(6, 118, W - 12, LINE);
  if (left) label(c, 8, 124, left, DIM);
  if (right) labelRight(c, W - 8, 124, right, DIM);
}

// ---- screens --------------------------------------------------------------
void drawHome(M5Canvas& c, const UiModel& m) {
  char buf[32];
  const Profile* p =
      (m.profiles && m.profileIndex < m.profileCount) ? &m.profiles[m.profileIndex] : nullptr;

  // Row 1: marker, profile number, battery.
  square(c, 8, 5, 6, ACCENT);
  snprintf(buf, sizeof buf, "PROFILE %02u/%02u", m.profileIndex + 1u, m.profileCount);
  label(c, 18, 5, buf, DIM);
  drawBattery(c, W - 8, 4, m.battery, m.charging);

  // Row 2: profile name, link tag.
  upper(buf, 16, p ? p->name : "");
  label(c, 8, 17, buf, INK, 2, 0);

  const char* tag = m.link == LinkState::SECURE      ? "SECURE"
                    : m.link == LinkState::HANDSHAKE ? "HANDSHAKE"
                                                     : "SEARCHING";
  int tw = textWidth(tag, 1, 1) + 12;
  int tx = W - 8 - tw;
  if (m.link == LinkState::SECURE) {
    c.fillRect(tx, 18, tw, 12, ACCENT);
    label(c, tx + 6, 20, tag, BG);
  } else {
    bool on = (millis() / 500) & 1;
    uint16_t col = m.link == LinkState::HANDSHAKE ? INK : DIM;
    c.drawRect(tx, 18, tw, 12, col == INK ? INK : LINE);
    label(c, tx + 6, 20, tag, col);
    if (on) square(c, tx - 7, 22, 4, ACCENT);
  }
  c.drawFastHLine(6, 35, W - 12, LINE);

  // Grid: column heads, 2 rows x 3 gestures.
  const int gx = 24, cw = 72, headY = 40, row0 = 51, rh = 32;
  static const char* const heads[3] = {"TAP", "DBL", "HOLD"};
  for (int g = 0; g < 3; ++g) label(c, gx + g * cw + 8, headY, heads[g], DIM);
  c.drawFastHLine(6, 50, W - 12, LINE);
  c.drawFastHLine(6, row0 + rh, W - 12, LINE);
  for (int g = 0; g < 3; ++g) c.drawFastVLine(gx + g * cw - 2, 37, 80, LINE);
  for (int r = 0; r < 2; ++r) {
    int y = row0 + r * rh;
    label(c, 10, y + rh / 2 - 8, r ? "B" : "A", ACCENT, 2, 0);
    for (int g = 0; g < 3; ++g) {
      int x = gx + g * cw;
      const Macro* mc = p ? &p->slots[r][g] : nullptr;
      int cy = y + rh / 2 - 4;
      if (mc && mc->count > 0) {
        square(c, x + 3, cy + 2, 3, ACCENT);
        upper(buf, 12, mc->label);
        label(c, x + 8, cy, buf, INK, 1, 0);
      } else {
        c.drawFastHLine(x + 8, cy + 4, 8, LINE);  // unassigned: dim dash
      }
    }
  }
  cornerTicks(c, 2, 2, W - 3, H - 3, LINE);
  frameFooter(c, "A+B  PROFILES", "NO. 0001");
}

void drawPicker(M5Canvas& c, const UiModel& m) {
  char buf[24];
  square(c, 8, 5, 6, ACCENT);
  label(c, 18, 5, "SELECT PROFILE", INK);
  snprintf(buf, sizeof buf, "%u/%u", m.highlighted + 1u, m.profileCount);
  labelRight(c, W - 8, 5, buf, DIM);
  c.drawFastHLine(6, 16, W - 12, LINE);

  const int rowH = 19, visible = 5, top = 20;
  int first = 0;
  if (m.profileCount > visible) {
    first = int(m.highlighted) - visible / 2;
    if (first < 0) first = 0;
    if (first > m.profileCount - visible) first = m.profileCount - visible;
  }
  for (int i = 0; i < visible && first + i < m.profileCount; ++i) {
    int idx = first + i, y = top + i * rowH;
    bool hi = idx == m.highlighted;
    uint16_t fg = hi ? BG : INK, sub = hi ? BG : DIM;
    if (hi) c.fillRect(6, y, W - 12, rowH - 2, ACCENT);
    snprintf(buf, sizeof buf, "%02d", idx + 1);
    label(c, 12, y + 5, buf, sub);
    upper(buf, 16, m.profiles ? m.profiles[idx].name : "");
    label(c, 34, y + 1, buf, fg, 2, 0);
    if (idx == m.profileIndex) labelRight(c, W - 12, y + 5, "ACTIVE", hi ? BG : ACCENT);
  }
  if (first > 0) c.fillTriangle(W - 4, 18, W - 7, 22, W - 1, 22, DIM);
  if (first + visible < m.profileCount) c.fillTriangle(W - 4, 116, W - 7, 112, W - 1, 112, DIM);
  frameFooter(c, "B NEXT  A ENGAGE  A+B CANCEL", nullptr);
}

void drawTransmit(M5Canvas& c, const UiModel& m) {
  char buf[24];
  bool blink = (millis() / 400) & 1;
  c.fillRect(0, 0, W, 15, ACCENT);
  label(c, 8, 4, "TRANSMITTING", BG, 1, 2);
  if (blink) square(c, W - 14, 4, 6, BG);
  else c.drawRect(W - 14, 4, 6, 6, BG);

  upper(buf, 12, m.macroLabel);
  int size = textWidth(buf, 3, 0) <= W - 16 ? 3 : 2;
  labelCenter(c, W / 2, 36, buf, INK, size, 0);

  const int bx = 12, by = 78, bw = W - 24, bh = 12;
  c.drawRect(bx, by, bw, bh, LINE);
  int fill = m.total ? int((uint32_t(m.sent) * (bw - 4)) / m.total) : 0;
  if (fill > bw - 4) fill = bw - 4;
  c.fillRect(bx + 2, by + 2, fill, bh - 4, ACCENT);
  for (int i = 1; i < 4; ++i) c.drawFastVLine(bx + bw * i / 4, by + bh + 1, 3, LINE);
  snprintf(buf, sizeof buf, "%u / %u", m.sent, m.total);
  labelCenter(c, W / 2, 98, buf, INK);
  frameFooter(c, nullptr, nullptr);
  labelCenter(c, W / 2, 124, "PRESS ANY BUTTON TO ABORT", DIM);
}
}  // namespace

// ---- Ui -------------------------------------------------------------------
Ui::Ui(M5GFX& display) : canvas_(&display) {}

void Ui::begin() {
  canvas_.setColorDepth(16);
  canvas_.createSprite(W, H);
  canvas_.fillScreen(BG);
  canvas_.pushSprite(0, 0);
}

void Ui::render(const UiModel& m) {
  canvas_.fillScreen(BG);
  switch (m.screen) {
    case Screen::HOME: drawHome(canvas_, m); break;
    case Screen::PICKER: drawPicker(canvas_, m); break;
    case Screen::TRANSMIT: drawTransmit(canvas_, m); break;
  }
  canvas_.pushSprite(0, 0);
}

void Ui::boot(const char* deviceName) {
  static const char* const lines[] = {"LINK INIT", "HID KEYBOARD", "PROFILES LOADED",
                                      "AWAITING HOST"};
  char name[32];
  upper(name, sizeof name, deviceName);
  const int steps = 5;
  for (int s = 0; s <= steps; ++s) {
    canvas_.fillScreen(BG);
    cornerTicks(canvas_, 2, 2, W - 3, H - 3, ACCENT);
    square(canvas_, 12, 12, 6, ACCENT);
    label(canvas_, 24, 12, "STICK-S3 / MACRO", INK, 1, 2);
    labelRight(canvas_, W - 12, 12, "NO. 0001", DIM);
    canvas_.drawFastHLine(12, 26, W - 24, LINE);
    label(canvas_, 12, 32, name, DIM);
    for (int i = 0; i < 4 && i < s; ++i) {
      bool done = i < s - 1 || s == steps;
      label(canvas_, 12, 48 + i * 12, ">", ACCENT);
      label(canvas_, 24, 48 + i * 12, lines[i], INK);
      labelRight(canvas_, W - 12, 48 + i * 12, done ? "OK" : "..", done ? ACCENT : DIM);
    }
    canvas_.drawRect(12, 112, W - 24, 8, LINE);
    canvas_.fillRect(14, 114, (W - 28) * s / steps, 4, ACCENT);
    canvas_.pushSprite(0, 0);
    delay(s == steps ? 100 : 120);
  }
}

void Ui::bondReset(uint32_t remainingMs, bool cleared) {
  canvas_.fillScreen(BG);
  canvas_.fillRect(0, 0, W, 15, ACCENT);
  label(canvas_, 8, 4, "WARNING", BG, 1, 2);
  labelRight(canvas_, W - 8, 4, "MAINTENANCE", BG);
  cornerTicks(canvas_, 2, 18, W - 3, H - 3, ACCENT);
  label(canvas_, 14, 26, "RESET", INK, 3, 1);
  label(canvas_, 14, 52, "BLUETOOTH", INK, 3, 1);
  char buf[32];
  if (!cleared) {
    uint32_t left = remainingMs > 3000 ? 3000 : remainingMs;
    snprintf(buf, sizeof buf, "KEEP A+B HELD %lu", (unsigned long)((left + 999) / 1000));
    label(canvas_, 14, 92, buf, ACCENT);
    canvas_.drawRect(14, 106, W - 28, 8, LINE);
    canvas_.fillRect(16, 108, int((W - 32) * (3000 - left) / 3000), 4, ACCENT);
  } else {
    label(canvas_, 14, 88, "BONDS CLEARED. RELEASE A+B.", ACCENT);
    label(canvas_, 14, 102, "FORGET DEVICE ON HOST,", INK);
    label(canvas_, 14, 114, "THEN PAIR.", INK);
  }
  canvas_.pushSprite(0, 0);
}

void Ui::fatal(const char* msg) {
  canvas_.fillScreen(BG);
  canvas_.fillRect(0, 0, W, 15, ALERT);
  label(canvas_, 8, 4, "FAULT", INK, 1, 2);
  labelRight(canvas_, W - 8, 4, "HALTED", INK);
  cornerTicks(canvas_, 2, 18, W - 3, H - 3, ALERT);
  char buf[40];  // wrap at 36 columns, 6 lines max
  const char* s = msg ? msg : "";
  for (int line = 0; line < 6 && *s; ++line) {
    size_t n = 0;
    while (*s && n < 36) buf[n++] = *s++;
    buf[n] = 0;
    label(canvas_, 14, 30 + line * 14, buf, INK, 1, 0);
  }
  canvas_.pushSprite(0, 0);
}
