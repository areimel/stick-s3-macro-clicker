#include "ProfileStore.h"
#include <Preferences.h>

uint8_t ProfileStore::load(uint8_t count) {
  if (count == 0) return 0;
  Preferences prefs;
  // A missing namespace (first boot) fails to open read-only; use profile 0.
  if (!prefs.begin("macro", /*readOnly=*/true)) return 0;
  const uint8_t value = prefs.getUChar("profile", 0);
  prefs.end();
  // A reflash with fewer profiles falls back to 0; leave last_ unset so the
  // corrected index is written on the next save.
  if (value >= count) return 0;
  last_ = value;
  return value;
}

bool ProfileStore::save(uint8_t idx) {
  // Skip write if unchanged (avoid flash wear).
  if (last_ == idx) return true;
  Preferences prefs;
  prefs.begin("macro", /*readOnly=*/false);
  bool ok = prefs.putUChar("profile", idx) > 0;
  prefs.end();
  if (ok) last_ = idx;
  return ok;
}
