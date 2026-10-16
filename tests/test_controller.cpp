#include "Controller.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

typedef ControllerEvent CE;

static SlotMask allAssigned() {
  SlotMask m;
  for (int b = 0; b < BUTTON_COUNT; b++)
    for (int g = 0; g < GESTURE_COUNT; g++) m.assigned[b][g] = true;
  return m;
}
static SlotMask noneAssigned() {
  SlotMask m = allAssigned();
  for (int b = 0; b < BUTTON_COUNT; b++)
    for (int g = 0; g < GESTURE_COUNT; g++) m.assigned[b][g] = false;
  return m;
}
static SlotMask singlesOnly() {
  SlotMask m = noneAssigned();
  m.assigned[BTN_MAIN][G_SINGLE] = m.assigned[BTN_SECONDARY][G_SINGLE] = true;
  return m;
}

// 1 ms tick simulator; collects every non-NONE event.
struct Sim {
  Controller c;
  bool a = false, b = false, ready = true, playing = false;
  uint32_t t;
  std::vector<CE> ev;
  SlotMask mask;
  explicit Sim(uint32_t start = 1000, const SlotMask& m = allAssigned(),
               uint8_t profiles = 3, uint8_t active = 0)
      : c(25, 250, 650, profiles), t(start), mask(m) {
    c.begin(false, false, t, active, m);
  }
  void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
      t++;
      CE e = c.update(a, b, t, ready, playing);
      if (e.type != CE::NONE) ev.push_back(e);
    }
  }
  void tapA(uint32_t holdMs = 50) { a = true; step(holdMs); a = false; step(60); }
  void tapB(uint32_t holdMs = 50) { b = true; step(holdMs); b = false; step(60); }
  void chord(uint32_t gap = 0) {
    a = true;
    step(gap ? gap : 1);
    b = true;
    step(100);
    a = b = false;
    step(100);
  }
  size_t n() const { return ev.size(); }
  void clear() { ev.clear(); }
};

int main() {
  // Chord opens picker when connected; releases emit nothing.
  {
    Sim s;
    s.step(50);
    s.chord();
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
    assert(s.ev[0].profile == 0);
    assert(s.c.mode() == Mode::PICKER && s.c.highlighted() == 0);
    s.step(500);
    assert(s.n() == 1);
  }
  // Chord opens picker when disconnected; highlight starts at active profile.
  {
    Sim s(1000, allAssigned(), 4, 2);
    s.ready = false;
    s.step(50);
    s.chord();
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
    assert(s.ev[0].profile == 2 && s.c.highlighted() == 2);
    s.step(500);
    assert(s.n() == 1);
  }
  // Staggered chord (A down, B 100 ms later) with A double+hold assigned: no stray single.
  {
    Sim s;
    s.step(10);
    s.chord(100);
    s.step(2000);
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
  }
  // Staggered chord, B first.
  {
    Sim s;
    s.step(10);
    s.b = true;
    s.step(120);
    s.a = true;
    s.step(100);
    s.a = s.b = false;
    s.step(2000);
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
  }
  // Chord beats a pending single of the same button (tap A, then chord within window).
  {
    Sim s;
    s.tapA(40);  // pending: double assigned; ~60 ms since release
    s.a = true;
    s.step(10);
    s.b = true;
    s.step(100);
    s.a = s.b = false;
    s.step(2000);
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
  }
  // No chord once a hold already fired: B behaves normally.
  {
    Sim s;
    s.step(10);
    s.a = true;
    s.step(700);
    assert(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].button == BTN_MAIN &&
           s.ev[0].gesture == G_HOLD);
    s.clear();
    s.b = true;
    s.step(50);
    s.b = false;
    s.step(400);  // B single (double assigned -> waits for window)
    assert(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].button == BTN_SECONDARY &&
           s.ev[0].gesture == G_SINGLE);
    s.a = false;
    s.step(500);
    assert(s.n() == 1 && s.c.mode() == Mode::HOME);
  }
  // Picker navigation wraps; select returns to HOME with the new profile.
  {
    Sim s(1000, allAssigned(), 3, 0);
    s.step(10);
    s.chord();
    s.clear();
    s.tapB();
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_MOVED && s.ev[0].profile == 1);
    s.tapB();
    assert(s.n() == 2 && s.ev[1].profile == 2);
    s.tapB();
    assert(s.n() == 3 && s.ev[2].profile == 0 && s.c.highlighted() == 0);
    s.tapB();
    s.tapB();  // highlight 2
    s.clear();
    s.tapA();
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_SELECTED && s.ev[0].profile == 2);
    assert(s.c.mode() == Mode::HOME && s.c.activeProfile() == 2);
    s.c.setActiveProfile(2, singlesOnly());
    s.mask = singlesOnly();
    s.clear();
    s.tapA();  // new mask: double unassigned -> immediate single, PLAY of profile 2
    assert(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].profile == 2 &&
           s.ev[0].button == BTN_MAIN && s.ev[0].gesture == G_SINGLE);
  }
  // Picker select with link not ready still works.
  {
    Sim s(1000, allAssigned(), 3, 0);
    s.ready = false;
    s.step(10);
    s.chord();
    s.clear();
    s.tapB();
    s.tapA();
    assert(s.n() == 2 && s.ev[1].type == CE::PICKER_SELECTED && s.ev[1].profile == 1);
  }
  // Picker taps are instant even when the profile has double+hold: tap A with no wait.
  {
    Sim s;
    s.step(10);
    s.chord();
    s.clear();
    s.b = true;
    s.step(40);
    s.b = false;
    s.step(30);  // far shorter than double window
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_MOVED);
  }
  // Holding in picker does not fire a hold; release acts as tap.
  {
    Sim s;
    s.step(10);
    s.chord();
    s.clear();
    s.b = true;
    s.step(1500);
    assert(s.n() == 0);
    s.b = false;
    s.step(60);
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_MOVED);
  }
  // Chord in picker cancels; active unchanged; releases emit nothing.
  {
    Sim s(1000, allAssigned(), 3, 1);
    s.step(10);
    s.chord();
    s.tapB();
    assert(s.c.highlighted() == 2);
    s.clear();
    s.chord();
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
    assert(s.c.mode() == Mode::HOME && s.c.activeProfile() == 1);
    s.step(1000);
    assert(s.n() == 1);
  }
  // Timeout cancels; any press refreshes it.
  {
    Sim s(1000, allAssigned(), 3, 1);
    s.step(10);
    s.chord();  // opens ~ (t) ; chord ends with 100 ms idle
    s.clear();
    s.step(9000);
    assert(s.n() == 0 && s.c.mode() == Mode::PICKER);
    s.tapB();  // refresh
    s.clear();
    s.step(9800);
    assert(s.n() == 0 && s.c.mode() == Mode::PICKER);
    s.step(400);
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
    assert(s.c.mode() == Mode::HOME && s.c.activeProfile() == 1);
  }
  // Exact timeout boundary measured from the opening tick.
  {
    Sim s;
    s.step(10);
    s.a = true;
    s.b = true;
    s.step(200);
    assert(s.n() == 1);
    const uint32_t opened = s.t;  // may be later than the open tick; bound it
    (void)opened;
    s.a = s.b = false;
    s.step(9000);
    assert(s.n() == 1);
    s.step(1500);
    assert(s.n() == 2 && s.ev[1].type == CE::PICKER_CANCELLED);
    s.step(200);
    assert(s.n() == 2);
  }
  // Timeout across millis() wraparound.
  {
    Sim s(UINT32_MAX - 3000);
    s.step(10);
    s.chord();
    s.clear();
    s.step(9000);
    assert(s.n() == 0 && s.c.mode() == Mode::PICKER);
    s.step(1500);
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
  }
  // Timeout while a button is held: release afterwards emits nothing.
  {
    Sim s;
    s.step(10);
    s.chord();
    s.clear();
    s.step(5000);
    s.b = true;
    s.step(10100);  // edge refreshed activity; hold for >10 s
    assert(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
    s.b = false;
    s.step(500);
    assert(s.n() == 1);
  }
  // PLAY only when ready and the slot is assigned.
  {
    Sim s(1000, singlesOnly());
    s.ready = false;
    s.step(10);
    s.tapA();
    s.tapB();
    assert(s.n() == 0);  // not ready: dropped
    s.ready = true;
    s.step(10);
    s.tapA();
    assert(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].button == BTN_MAIN &&
           s.ev[0].gesture == G_SINGLE && s.ev[0].profile == 0);
    s.tapB();
    assert(s.n() == 2 && s.ev[1].button == BTN_SECONDARY);
  }
  // Unassigned slot emits nothing (single unassigned, hold assigned only).
  {
    SlotMask m = noneAssigned();
    m.assigned[BTN_MAIN][G_HOLD] = true;
    Sim s(1000, m);
    s.step(10);
    s.tapA();
    s.tapB();
    assert(s.n() == 0);
    s.a = true;
    s.step(700);
    assert(s.n() == 1 && s.ev[0].gesture == G_HOLD);
    s.a = false;
    s.step(500);
    assert(s.n() == 1);
  }
  // Double gesture plays; single waits out the double window.
  {
    Sim s;
    s.step(10);
    s.tapA(50);
    s.tapA(50);
    assert(s.n() == 1 && s.ev[0].gesture == G_DOUBLE && s.ev[0].button == BTN_MAIN);
    s.clear();
    s.tapA(50);
    s.step(400);
    assert(s.n() == 1 && s.ev[0].gesture == G_SINGLE);
  }
  // Disabling single in the mask but keeping double: a lone tap plays nothing.
  {
    SlotMask m = noneAssigned();
    m.assigned[BTN_MAIN][G_DOUBLE] = true;
    Sim s(1000, m);
    s.step(10);
    s.tapA(50);
    s.step(500);
    assert(s.n() == 0);
  }
  // Readiness change discards a held button; a pre-connect hold cannot leak.
  {
    Sim s;
    s.ready = false;
    s.step(10);
    s.a = true;
    s.step(1000);  // held for a long time while not ready
    assert(s.n() == 0);
    s.ready = true;
    s.step(1000);
    assert(s.n() == 0);  // still held after becoming ready: no hold event
    s.a = false;
    s.step(500);
    assert(s.n() == 0);  // release suppressed
    s.tapA();
    s.step(400);
    assert(s.n() == 1 && s.ev[0].gesture == G_SINGLE);
  }
  // Losing the link mid-hold discards pending gestures.
  {
    Sim s;
    s.step(10);
    s.tapA(50);  // pending single (double assigned)
    s.ready = false;
    s.step(1000);
    assert(s.n() == 0);
  }
  // ABORT during playing; release afterwards emits nothing; no PLAY while playing.
  {
    Sim s;
    s.step(10);
    s.playing = true;
    s.step(10);
    s.a = true;
    s.step(100);
    assert(s.n() == 1 && s.ev[0].type == CE::ABORT);
    s.step(1000);  // held through a would-be hold; playing still true
    assert(s.n() == 1);
    s.playing = false;
    s.a = false;
    s.step(500);
    assert(s.n() == 1);  // release emits nothing
    s.tapB();
    s.step(400);
    assert(s.n() == 2 && s.ev[1].type == CE::PLAY);
  }
  // A short tap while playing aborts and does not also play.
  {
    Sim s(1000, singlesOnly());
    s.step(10);
    s.playing = true;
    s.tapB();
    assert(s.n() == 1 && s.ev[0].type == CE::ABORT);
    s.playing = false;
    s.step(500);
    assert(s.n() == 1);
  }
  // Button pressed before playback starts is not an abort edge (hold already fired
  // PLAY, playback begins, release must be quiet).
  {
    Sim s;
    s.step(10);
    s.a = true;
    s.step(700);
    assert(s.n() == 1 && s.ev[0].type == CE::PLAY);
    s.playing = true;
    s.step(100);
    assert(s.n() == 1);
    s.a = false;
    s.step(200);
    assert(s.n() == 1);
  }
  // Buttons held at begin() are blocked and cannot chord.
  {
    Controller c(25, 250, 650, 3);
    c.begin(true, true, 100, 0, allAssigned());
    uint32_t t = 100;
    for (int i = 0; i < 500; i++) {
      CE e = c.update(true, true, ++t, true, false);
      assert(e.type == CE::NONE);
    }
    assert(c.mode() == Mode::HOME);
  }
  std::cout << "Controller tests passed\n";
}
