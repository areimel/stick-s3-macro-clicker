#pragma once
#include <stdint.h>

enum class GestureEvent : uint8_t { NONE = 0, SINGLE, DOUBLE, HOLD };

// Framework-independent, debounced one-shot gestures. Call update() on every
// loop tick. The engine only classifies presses; it knows nothing about what an
// event does. A single press is always reported; whether it maps to an action
// is the caller's decision.
//
// setAssigned(dbl, hold) tells the engine which richer gestures are wanted:
//  - double unassigned: SINGLE fires immediately on release (no wait window).
//  - hold unassigned: a long press is an ordinary tap and fires SINGLE (or
//    starts a double window) on release.
// Changing assignments mid-gesture will not crash or corrupt state, but the
// in-flight gesture may be classified under either setting. Callers should
// call cancel() or reset() around profile or mode changes.
class Gesture {
 public:
  Gesture(uint32_t debounce, uint32_t doubleMs, uint32_t longMs)
      : debounce_(debounce), doubleMs_(doubleMs), longMs_(longMs) {}

  void setAssigned(bool dbl, bool hold) {
    double_ = dbl;
    hold_ = hold;
  }

  // Reset when connection readiness changes. A button already down must first
  // be released; it cannot leak a stale gesture into the next connection.
  void reset(bool rawPressed, uint32_t now) {
    raw_ = pressed_ = rawPressed;
    changedAt_ = pressedAt_ = releasedAt_ = now;
    blocked_ = rawPressed;
    held_ = pending_ = second_ = false;
  }

  // Drop any pending single/double and, if the button is currently down
  // (debounced), swallow the rest of this press: no event on release.
  void cancel() {
    pending_ = second_ = held_ = false;
    blocked_ = pressed_;
  }

  // Debounced button state.
  bool pressed() const { return pressed_; }
  // True from the moment a hold event fired until the button is released.
  bool holdFired() const { return held_; }

  GestureEvent update(bool rawPressed, uint32_t now) {
    if (rawPressed != raw_) {
      raw_ = rawPressed;
      changedAt_ = now;
    }
    GestureEvent action = GestureEvent::NONE;
    // Once the second press is debounced within the window, await its release
    // or hold even if that takes longer than the double-click window.
    if (pending_ && !second_ && elapsed(now, releasedAt_) >= doubleMs_) {
      pending_ = false;
      action = GestureEvent::SINGLE;
    }
    if (raw_ != pressed_ && elapsed(now, changedAt_) >= debounce_) {
      pressed_ = raw_;
      if (pressed_) {
        pressedAt_ = now;
        held_ = false;
        second_ = pending_;
      } else if (blocked_) {
        blocked_ = false;
      } else if (held_) {
        held_ = false;
      } else if (second_) {
        pending_ = second_ = false;
        action = GestureEvent::DOUBLE;
      } else if (double_) {
        pending_ = true;
        releasedAt_ = now;
      } else {
        action = GestureEvent::SINGLE;
      }
    }
    if (pressed_ && raw_ && !blocked_ && !held_ && hold_ &&
        elapsed(now, pressedAt_) >= longMs_) {
      held_ = true;
      pending_ = second_ = false;
      action = GestureEvent::HOLD;
    }
    return action;
  }

 private:
  static uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }
  uint32_t debounce_, doubleMs_, longMs_;
  bool double_ = false, hold_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0, releasedAt_ = 0;
  bool raw_ = false, pressed_ = false, blocked_ = false;
  bool held_ = false, pending_ = false, second_ = false;
};
