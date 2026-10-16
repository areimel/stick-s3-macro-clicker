#pragma once
#include <stdint.h>
#include "Macro.h"

// Walks a Macro's strokes one at a time. Pure logic: no Arduino, no BLE.
//
// The caller polls next() every loop with the transport's idle() state. A real
// stroke is handed out only while the transport is idle, so pacing (hold and
// release gap) is owned by the transport. Pause strokes consume no transport
// time: their delay starts once the transport is idle after the previous
// stroke. The player stays active until the last stroke has been handed out and
// the transport has gone idle again, so a UI can cover the final release.
// After abort() nothing is ever handed out again until a new start().
class MacroPlayer {
 public:
  // Rejected (false) when a macro is already active or the macro is empty.
  bool start(const Macro& macro, uint32_t now) {
    (void)now;
    if (active_ || macro.count == 0 || macro.strokes == nullptr) return false;
    macro_ = &macro;
    index_ = 0;
    pauseStarted_ = false;
    active_ = true;
    return true;
  }

  // Returns true and fills out when a keystroke must be sent right now.
  bool next(uint32_t now, bool transportIdle, Stroke& out) {
    if (!active_) return false;
    while (index_ < macro_->count) {
      const Stroke& s = macro_->strokes[index_];
      if (isPause(s)) {
        if (!transportIdle && !pauseStarted_) return false;
        if (!pauseStarted_) {
          pauseStarted_ = true;
          pauseStart_ = now;
        }
        if (uint32_t(now - pauseStart_) < s.delayMs) return false;
        pauseStarted_ = false;
        ++index_;
        continue;
      }
      if (!transportIdle) return false;
      out = s;
      ++index_;
      return true;
    }
    if (transportIdle) active_ = false;  // everything sent and released
    return false;
  }

  void abort() {
    active_ = false;
    pauseStarted_ = false;
  }

  bool active() const { return active_; }
  uint16_t sent() const { return index_; }  // strokes handed out or pauses elapsed
  uint16_t total() const { return macro_ ? macro_->count : 0; }
  const Macro* macro() const { return macro_; }

 private:
  const Macro* macro_ = nullptr;
  uint16_t index_ = 0;
  bool active_ = false;
  bool pauseStarted_ = false;
  uint32_t pauseStart_ = 0;
};
