#pragma once
#include <stdint.h>

class ProfileStore {
 public:
  // Returns saved index clamped to [0, count); 0 if nothing saved, count==0, or NVS fails.
  uint8_t load(uint8_t count);

  // Persists idx; skips the write if it equals the last loaded/saved value
  // (avoid flash wear). Returns false on failure.
  bool save(uint8_t idx);

 private:
  int16_t last_ = -1;
};
