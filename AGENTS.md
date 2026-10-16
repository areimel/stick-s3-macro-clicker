# Stick-S3 Macro Clicker

## Working agreements

- Target the M5Stack Stick-S3 (ESP32-S3), using the pinned PlatformIO/Arduino,
  M5Unified, M5GFX, and NimBLE dependencies in `platformio.ini`.
- Keep this a small, dependable macro device. Prefer clear modules over extra
  features. Do not add network services or runtime configuration systems without
  a request. Profiles stay compile-time; the only runtime state is the remembered
  active profile index.
- `config.yaml` is the source of truth for profiles, macros, gesture timing,
  typing speed, Bluetooth name, and display brightness. Validate it at build time;
  never hardcode configurable mappings in firmware or commit generated headers
  (`include/generated_config.h` is generated and ignored).
- Never put secrets in `config.yaml`, tests, or fixtures. It is embedded in the
  published firmware manifest and text macros are typed in clear.
- Module boundaries:
  - `include/Macro.h` is the shared data contract (`Stroke`, `Macro`, `Profile`).
  - `scripts/generate_config.py` validates YAML and compiles key combos, text and
    labels into strokes; firmware never parses key names or text.
  - `Gesture.h`, `Controller.h`, `MacroPlayer.h` are pure, header-only logic with
    host C++ tests; no Arduino or hardware includes.
  - `BleKeyboard.h`/`src/BleKeyboard.cpp` is the HID transport only (one
    keystroke at a time, paced by `keystroke_ms`).
  - `Theme.h`/`Ui.h`/`src/Ui.cpp` render a plain `UiModel`; no input logic.
  - `ProfileStore` persists the active profile in NVS.
  - `src/main.cpp` is wiring: controller -> player -> keyboard -> ui.
  - `web/` is the independent Astro flasher; it reads config from the packaged
    manifest rather than duplicating it.
- Preserve: one-shot long presses, release suppression after a long press,
  explicit HID key release, discarding input during disconnection, aborting
  playback on any new press, the A+B chord (which cancels pending gestures so
  releases fire nothing), and never replaying or resuming a macro after a
  disconnect or abort. The picker must keep working while disconnected; macros
  play only when the link is ready.
- Keep Bluetooth awake for use. Do not introduce automatic sleep without
  considering missed input and reconnect delay.
- Changing the HID report descriptor requires telling users to forget and re-pair.
- Use a TODO list and focused local commits with descriptive titles and bodies.
  Keep parallel workstreams on distinct files; coordinate shared interfaces.
- Preserve user changes. Do not push commits unless requested.

## Build and validation

Create a Python virtual environment and install `requirements.txt` into it.
Use `.venv/bin/pio run -e sticks3` for a firmware build and
`.venv/bin/pio device list` to identify serial devices. On Windows the same tools
are `.venv\Scripts\pio` and `.venv\Scripts\python`. The PlatformIO Python
environment must contain PyYAML for the pre-build generator.

Run `python -m unittest discover -s tests -v` (after a firmware build, because
packaging tests use PlatformIO's esptool) and the four host C++ tests listed in
README.md (gesture, controller, player; BLE with `-Itests/ble` and
`src/BleKeyboard.cpp`) when changing those behaviors. Windows has no native
compiler by default: use WSL or `docker run --rm -v "${PWD}:/src" -w /src gcc:14
sh -c "..."`. For web changes run `npm ci`, `npm run check`, `npm test`, and
`npm run build` in `web/`. Verify a firmware build after integration. Do not
claim a successful compile proves Bluetooth pairing, button identity, display
rendering, typing reliability, or battery runtime.

Before flashing, reidentify the intended USB device and verify its chip/flash
identity. Flash only within the user's authorization. Port names are examples,
not stable device identifiers. Close serial monitors before uploading. Report
physical checks as pending unless observed or confirmed by the user.

Keep README setup, YAML examples, gesture and picker semantics, validation limits,
and recovery instructions aligned with `config.yaml` and
`scripts/generate_config.py`. Document useful verification results and known
limitations without making unmeasured battery-life claims.

<!-- CODEGRAPH_START -->
## CodeGraph

In repositories indexed by CodeGraph (a `.codegraph/` directory exists at the repo root), reach for it BEFORE grep/find or reading files when you need to understand or locate code:

- **MCP tool** (when available): `codegraph_explore` answers most code questions in one call — the relevant symbols' verbatim source plus the call paths between them, including dynamic-dispatch hops grep can't follow. Name a file or symbol in the query to read its current line-numbered source. If it's listed but deferred, load it by name via tool search.
- **Shell** (always works): `codegraph explore "<symbol names or question>"` prints the same output.

If there is no `.codegraph/` directory, skip CodeGraph entirely — indexing is the user's decision.
<!-- CODEGRAPH_END -->
