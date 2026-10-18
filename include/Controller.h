#pragma once
#include <stdint.h>
#include "Gesture.h"
#include "Macro.h"

// Pure, host-testable input controller. Owns one Gesture per button (main =
// BtnA, secondary = BtnB), the A+B chord, the HOME/PICKER mode and the picker
// timeout. No hardware, no Arduino.
//
// Usage: call begin() once, then update() every loop tick with the raw button
// states, millis(), whether the BLE link is ready and whether a macro is
// currently playing. update() returns at most one event per call. Priority:
// chord (PICKER_OPENED / PICKER_CANCELLED) > ABORT > everything else.
//
//  - HOME: PLAY{button, gesture, profile} when the link is ready, nothing is
//    playing and the slot is assigned in the SlotMask. While the link is not
//    ready, gestures run with double/hold disabled and every event is dropped;
//    gestures are reset on every readiness change, so a button held across a
//    (re)connection must be released first.
//  - Playing: a new debounced press emits ABORT and cancels both gestures (the
//    release emits nothing). No PLAY is emitted while playing. The caller must
//    also abort its player on PICKER_OPENED if a macro was still playing.
//  - Chord: both buttons debounced-down while neither has fired a hold. Works
//    regardless of link state. Opens the picker (highlight = active profile) in
//    HOME, cancels it in PICKER. Both gestures are cancelled so the releases and
//    any pending single/double emit nothing. Both buttons must be released
//    before another chord is recognised.
//  - PICKER: double/hold disabled. B tap -> PICKER_MOVED (highlight+1, wrapping),
//    A tap -> PICKER_SELECTED{profile} and return to HOME, kPickerTimeoutMs
//    without any press -> PICKER_CANCELLED. After PICKER_SELECTED the caller
//    must call setActiveProfile(profile, newMask) before the next update().

struct SlotMask {
  bool assigned[BUTTON_COUNT][GESTURE_COUNT];
};

enum class Mode : uint8_t { HOME, PICKER };

struct ControllerEvent {
  enum Type : uint8_t {
    NONE,
    PLAY,              // button, gesture, profile valid
    ABORT,             // user pressed a button while a macro was playing
    PICKER_OPENED,     // profile = highlighted (= active)
    PICKER_MOVED,      // profile = new highlight
    PICKER_SELECTED,   // profile = newly active profile
    PICKER_CANCELLED   // chord or timeout; active profile unchanged
  };
  Type type;
  uint8_t button;   // Button, PLAY only
  uint8_t gesture;  // GestureKind, PLAY only
  uint8_t profile;  // see above
};

class Controller {
 public:
  static constexpr uint32_t kPickerTimeoutMs = 10000;

  Controller(uint32_t debounce, uint32_t doubleMs, uint32_t longMs,
             uint8_t profileCount)
      : main_(debounce, doubleMs, longMs),
        secondary_(debounce, doubleMs, longMs),
        profileCount_(profileCount ? profileCount : 1) {}

  // Link is assumed not ready until update() reports otherwise. Buttons already
  // down at begin() must be released before they act (and cannot form a chord).
  void begin(bool rawA, bool rawB, uint32_t now, uint8_t activeProfile,
             const SlotMask& mask) {
    mode_ = Mode::HOME;
    active_ = highlight_ = activeProfile % profileCount_;
    mask_ = mask;
    ready_ = false;
    lastActivity_ = now;
    resetGestures(rawA, rawB, now);
    applyAssignments();
  }

  // Call after PICKER_SELECTED (and whenever the active profile changes).
  void setActiveProfile(uint8_t idx, const SlotMask& mask) {
    active_ = highlight_ = idx % profileCount_;
    mask_ = mask;
    main_.cancel();
    secondary_.cancel();
    applyAssignments();
  }

  ControllerEvent update(bool rawA, bool rawB, uint32_t now, bool linkReady,
                         bool playing) {
    if (linkReady != ready_) {
      ready_ = linkReady;
      resetGestures(rawA, rawB, now);
      applyAssignments();
    }
    const GestureEvent ev[2] = {main_.update(rawA, now),
                                secondary_.update(rawB, now)};
    bool edge[2];
    const bool down[2] = {main_.pressed(), secondary_.pressed()};
    for (int i = 0; i < 2; i++) {
      edge[i] = down[i] && !prev_[i];
      prev_[i] = down[i];
    }
    const bool anyEdge = edge[0] || edge[1];

    if (!down[0] && !down[1]) latched_ = false;
    if (down[0] && down[1] && !latched_ && !main_.holdFired() &&
        !secondary_.holdFired()) {
      latched_ = true;
      cancelAll();
      if (mode_ == Mode::HOME) {
        mode_ = Mode::PICKER;
        highlight_ = active_;
        lastActivity_ = now;
        applyAssignments();
        return make(ControllerEvent::PICKER_OPENED, highlight_);
      }
      mode_ = Mode::HOME;
      applyAssignments();
      return make(ControllerEvent::PICKER_CANCELLED, active_);
    }

    if (mode_ == Mode::PICKER) {
      if (anyEdge) lastActivity_ = now;
      if (uint32_t(now - lastActivity_) >= kPickerTimeoutMs) {
        mode_ = Mode::HOME;
        cancelAll();
        applyAssignments();
        return make(ControllerEvent::PICKER_CANCELLED, active_);
      }
      if (ev[0] == GestureEvent::SINGLE) {
        active_ = highlight_;
        mode_ = Mode::HOME;
        applyAssignments();  // caller follows with setActiveProfile()
        return make(ControllerEvent::PICKER_SELECTED, active_);
      }
      if (ev[1] == GestureEvent::SINGLE) {
        highlight_ = (highlight_ + 1) % profileCount_;
        return make(ControllerEvent::PICKER_MOVED, highlight_);
      }
      return make(ControllerEvent::NONE, active_);
    }

    if (playing && anyEdge) {
      cancelAll();
      return make(ControllerEvent::ABORT, active_);
    }
    if (!ready_ || playing) return make(ControllerEvent::NONE, active_);
    for (int i = 0; i < 2; i++) {
      int kind = -1;
      if (ev[i] == GestureEvent::SINGLE) kind = G_SINGLE;
      else if (ev[i] == GestureEvent::DOUBLE) kind = G_DOUBLE;
      else if (ev[i] == GestureEvent::HOLD) kind = G_HOLD;
      if (kind >= 0 && mask_.assigned[i][kind]) {
        ControllerEvent e = make(ControllerEvent::PLAY, active_);
        e.button = uint8_t(i);
        e.gesture = uint8_t(kind);
        return e;
      }
    }
    return make(ControllerEvent::NONE, active_);
  }

  Mode mode() const { return mode_; }
  uint8_t activeProfile() const { return active_; }
  uint8_t highlighted() const { return highlight_; }

 private:
  static ControllerEvent make(ControllerEvent::Type t, uint8_t profile) {
    ControllerEvent e;
    e.type = t;
    e.button = 0;
    e.gesture = 0;
    e.profile = profile;
    return e;
  }

  void cancelAll() {
    main_.cancel();
    secondary_.cancel();
  }

  void resetGestures(bool rawA, bool rawB, uint32_t now) {
    main_.reset(rawA, now);
    secondary_.reset(rawB, now);
    prev_[0] = main_.pressed();
    prev_[1] = secondary_.pressed();
    // Buttons already down at a reset must not form a chord.
    latched_ = prev_[0] && prev_[1];
  }

  void applyAssignments() {
    const bool rich = mode_ == Mode::HOME && ready_;
    main_.setAssigned(rich && mask_.assigned[BTN_MAIN][G_DOUBLE],
                      rich && mask_.assigned[BTN_MAIN][G_HOLD]);
    secondary_.setAssigned(rich && mask_.assigned[BTN_SECONDARY][G_DOUBLE],
                           rich && mask_.assigned[BTN_SECONDARY][G_HOLD]);
  }

  Gesture main_, secondary_;
  uint8_t profileCount_;
  Mode mode_ = Mode::HOME;
  uint8_t active_ = 0, highlight_ = 0;
  SlotMask mask_ = {};
  bool ready_ = false;
  bool prev_[2] = {false, false};
  bool latched_ = false;
  uint32_t lastActivity_ = 0;
};
