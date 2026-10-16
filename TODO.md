# Macro profiles rework

Reworking the Stick-S3 Deck Clicker into the Stick-S3 Macro Clicker.

- [x] Import the web flasher, packaging script, and CI/release workflows from the deck clicker.
- [x] Profile-based YAML schema and generator: key combos, text, sequences, delays, labels, limits.
- [x] Gesture engine refactor, A+B chord controller, and profile picker.
- [x] Macro player, paced single-stroke BLE transport, widened HID map, abort on press/disconnect.
- [x] Remembered active profile (NVS) clamped when profiles shrink.
- [x] Agency-styled display: boot, HOME, PICKER, TRANSMIT, bond reset screens.
- [x] Host tests: configuration/packaging (Python) and gesture, controller, player, BLE (C++).
- [x] Rebrand the web flasher for macro profiles.
- [x] Rewrite README, AGENTS.md, and TODO.md.
- [ ] User: forget the old pairing, re-pair, and confirm the keyboard reaches ready.
- [ ] User: verify every gesture and the A+B picker on the device (including abort and the 10 s timeout).
- [ ] User: type the long CLAUDE prompts at `keystroke_ms: 15`; raise it if characters drop.
- [ ] User: check display legibility at the configured brightness.
- [ ] User: verify web flashing (install, normal reflash, clean install) on hardware.
- [ ] User: authorize pushing, tagging, and publishing the first release and Pages site.
- [ ] User: measure battery runtime if it matters (no figure is claimed).
