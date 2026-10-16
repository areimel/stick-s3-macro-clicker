#include "Gesture.h"
#include <cassert>
#include <cstdint>
#include <iostream>

static const GestureEvent NONE = GestureEvent::NONE, TAP = GestureEvent::SINGLE,
                          DOUBLE = GestureEvent::DOUBLE, HOLD = GestureEvent::HOLD;

// Times supplied to press/release denote raw electrical edges; debounce is 25ms.
static void press(Gesture& g, uint32_t t) {
  assert(g.update(true, t) == NONE);
  assert(g.update(true, t + 25) == NONE);
}
static GestureEvent release(Gesture& g, uint32_t t) {
  assert(g.update(false, t) == NONE);
  return g.update(false, t + 25);
}
static Gesture make(bool dbl, bool hold) {
  Gesture g(25, 250, 650);
  g.setAssigned(dbl, hold);
  return g;
}

int main() {
  {
    Gesture g = make(false, false);
    assert(g.update(true, 10) == NONE);
    assert(g.update(false, 20) == NONE); // noise never becomes a press
    assert(g.update(false, 45) == NONE);
    assert(!g.pressed());
    press(g, 100);
    assert(g.pressed());
    assert(release(g, 150) == TAP);
    press(g, 180);
    assert(release(g, 230) == TAP); // no double assignment: two immediate taps
    press(g, 300);
    assert(g.update(true, 2000) == NONE);
    assert(!g.holdFired());
    assert(release(g, 2100) == TAP); // unassigned hold is ordinary tap
  }
  {
    Gesture g = make(false, true);
    press(g, 10);
    assert(g.update(true, 684) == NONE);
    assert(!g.holdFired());
    assert(g.update(true, 685) == HOLD);
    assert(g.holdFired());
    assert(g.update(true, 2000) == NONE);
    assert(release(g, 2100) == NONE);
    assert(!g.holdFired());
    press(g, 2200);
    assert(release(g, 2300) == TAP);
  }
  {
    Gesture g = make(false, true);
    press(g, 10);
    assert(g.update(false, 680) == NONE);
    assert(g.update(false, 685) == NONE); // release before threshold cannot hold
    assert(g.update(false, 705) == TAP);
  }
  {
    Gesture g = make(true, true);
    press(g, 10);
    assert(release(g, 60) == NONE);
    assert(g.update(false, 334) == NONE);
    assert(g.update(false, 335) == TAP);
    assert(g.update(false, 500) == NONE);
    press(g, 600);
    assert(release(g, 650) == NONE);
    press(g, 700);
    assert(release(g, 800) == DOUBLE);
    assert(g.update(false, 1100) == NONE);
  }
  {
    Gesture g = make(true, true);
    press(g, 10);
    assert(release(g, 60) == NONE);
    press(g, 100);
    assert(g.update(true, 400) == NONE); // second press locks arbitration
    assert(g.update(true, 775) == HOLD); // second hold wins
    assert(release(g, 900) == NONE);
    assert(g.update(false, 1500) == NONE);
  }
  {
    Gesture g = make(true, true);
    press(g, 10);
    assert(release(g, 60) == NONE);
    g.reset(false, 100); // pending action discarded on disconnect
    assert(g.update(false, 500) == NONE);
    g.reset(true, 600); // held button across reconnect must release first
    assert(g.pressed());
    assert(g.update(true, 2000) == NONE);
    assert(release(g, 2100) == NONE);
    press(g, 2200);
    assert(g.update(true, 2875) == HOLD);
    assert(release(g, 3000) == NONE);
  }
  {
    Gesture g = make(true, true);
    const uint32_t start = UINT32_MAX - 100;
    g.reset(false, start);
    press(g, start + 10);
    assert(release(g, start + 60) == NONE);
    assert(g.update(false, start + 335) == TAP); // wrapped unsigned clock
    press(g, start + 400);
    assert(g.update(true, start + 1075) == HOLD);
  }
  {
    Gesture a = make(false, false), b = make(false, true);
    press(a, 10);
    press(b, 10);
    assert(release(a, 100) == TAP);
    assert(b.update(true, 685) == HOLD);
    assert(release(b, 700) == NONE);
  }
  // cancel(): drops a pending single so it never fires
  {
    Gesture g = make(true, true);
    press(g, 10);
    assert(release(g, 60) == NONE); // pending
    g.cancel();
    assert(g.update(false, 400) == NONE);
    assert(g.update(false, 2000) == NONE);
    press(g, 3000);
    assert(release(g, 3050) == NONE); // new pending works normally
    assert(g.update(false, 3400) == TAP);
  }
  // cancel(): drops a second press in progress (no double on release)
  {
    Gesture g = make(true, true);
    press(g, 10);
    assert(release(g, 60) == NONE);
    press(g, 100); // second press
    g.cancel();
    assert(release(g, 200) == NONE);
    assert(g.update(false, 1000) == NONE);
  }
  // cancel(): blocks a held button until release; no tap, no hold
  {
    Gesture g = make(false, true);
    press(g, 10);
    g.cancel();
    assert(g.pressed());
    assert(g.update(true, 2000) == NONE); // no hold after cancel
    assert(release(g, 2100) == NONE);
    press(g, 2200);
    assert(release(g, 2300) == TAP); // fully recovered
  }
  // cancel() after a hold fired: holdFired cleared, release silent
  {
    Gesture g = make(false, true);
    press(g, 10);
    assert(g.update(true, 685) == HOLD);
    g.cancel();
    assert(!g.holdFired());
    assert(release(g, 800) == NONE);
    press(g, 900);
    assert(release(g, 950) == TAP);
  }
  // cancel() while idle is a no-op
  {
    Gesture g = make(false, false);
    g.cancel();
    press(g, 10);
    assert(release(g, 60) == TAP);
  }
  // setAssigned between gestures takes effect immediately
  {
    Gesture g = make(false, false);
    press(g, 10);
    assert(release(g, 60) == TAP); // immediate single
    g.setAssigned(true, false);
    press(g, 200);
    assert(release(g, 250) == NONE); // now waits for double window
    assert(g.update(false, 525) == TAP);
    g.setAssigned(false, true);
    press(g, 600);
    assert(g.update(true, 1275) == HOLD);
    assert(release(g, 1400) == NONE);
    g.setAssigned(false, false);
    press(g, 1500);
    assert(g.update(true, 3000) == NONE);
    assert(release(g, 3100) == TAP);
  }
  // setAssigned mid-gesture is safe: disabling hold while held keeps release silent
  {
    Gesture g = make(false, true);
    press(g, 10);
    assert(g.update(true, 685) == HOLD);
    g.setAssigned(false, false);
    assert(release(g, 800) == NONE);
    assert(!g.holdFired());
  }
  // disabling double while single is pending: the pending single still resolves
  {
    Gesture g = make(true, false);
    press(g, 10);
    assert(release(g, 60) == NONE);
    g.setAssigned(false, false);
    assert(g.update(false, 335) == TAP);
    assert(g.update(false, 1000) == NONE);
  }
  std::cout << "Gesture tests passed\n";
}
