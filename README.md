# M5Stack Stick-S3 Macro Clicker

A pocket Bluetooth macro keyboard. Two buttons, three gestures each (tap,
double-tap, hold), and switchable **profiles** give you up to six macros per
profile on the device and up to nine profiles. Each macro can be a key combo
(`CMD+SHIFT+4`), a block of typed text, or a timed sequence of both. Use it for
Claude Code prompts, copy/paste/undo shortcuts, slide control, or anything else a
keyboard can do. The screen shows the active profile, what each button does,
Bluetooth status, and battery level.

This project targets the **M5Stack Stick-S3 (ESP32-S3)**, not the older
M5StickC/M5StickC Plus models. It uses PlatformIO, Arduino, M5Unified, and
NimBLE. Source: <https://github.com/areimel/stick-s3-macro-clicker>.

> **SECURITY: never put secrets in macros.** `config.yaml` is compiled into the
> firmware and is also embedded verbatim in the published firmware manifest
> (`manifest.json`) that the web flasher and GitHub Releases serve. Text macros
> are typed in clear text into whatever has keyboard focus. Do not put
> passwords, tokens, API keys, recovery codes, or anything else sensitive in
> `config.yaml`, in any form.

This project was reworked from the "Stick-S3 Deck Clicker" presentation remote.
Its original behavior survives as the **DECK** profile.

## Install from your browser

The Astro app in `web/` installs the latest official firmware over USB, with no
PlatformIO installation needed by visitors. The public GitHub Pages release is
published separately (expected at
`https://areimel.github.io/stick-s3-macro-clicker/` once the first release is
published); until then, use the local preview instructions below.

1. Open the flasher in desktop Chrome or Edge and connect a **Stick-S3** with a
   USB **data** cable. Close serial monitors and other apps using its port.
2. Choose **Connect device** and select its current USB serial port. The flasher
   checks for an ESP32-S3 with 8 MB flash. These checks identify compatible
   silicon, not the board model; confirm that your physical device is a Stick-S3.
3. Review the firmware and select **Install firmware**. Keep the device connected
   until writing and verification finish.
4. Follow the pairing instructions on the page and try each profile.

Normal reflashing preserves the pairing-storage region for this firmware's
partition layout. The optional **Clean installation** erases all device storage,
including Bluetooth bonds and the remembered profile; forget the old
computer-side Bluetooth entry and pair again afterward. Installing the official
firmware replaces any locally compiled custom macros with the release's
settings.

The flasher needs HTTPS or localhost and browser Web Serial support. Desktop
Chrome and Edge are the initial targets; mobile flashing is not supported in this
version. If automatic reset fails, follow the page's manual restart instructions.
The command-line build/upload process below remains available for recovery and
custom YAML configurations.

## Build and flash from the command line

Install Python 3 and connect the Stick-S3 with a USB **data** cable. From this
repository (macOS/Linux/WSL):

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/pio run -e sticks3
.venv/bin/pio device list
```

On Windows (PowerShell), the virtual environment tools live in `Scripts`:

```powershell
python -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
.venv\Scripts\pio run -e sticks3
.venv\Scripts\pio device list
```

Find the Stick-S3 serial port in the device list, then replace the example port
below with its current name (`/dev/cu.usbmodem11301` on macOS, `COM5` on
Windows; substitute `.venv\Scripts\pio` on Windows):

```sh
.venv/bin/pio run -e sticks3 -t upload --upload-port /dev/cu.usbmodem11301
.venv/bin/pio device monitor --port /dev/cu.usbmodem11301 --baud 115200
```

The serial-port name can change after unplugging, resetting, or entering download
mode. Exit the monitor with Ctrl+C before uploading again. Firmware dependencies
are pinned in `platformio.ini`. **PyYAML must be installed in the same Python
environment that runs PlatformIO** (`requirements.txt` does this): the pre-build
hook uses it to validate `config.yaml` and generate the firmware's macro tables.

## Pair and use

1. Power on the Stick-S3 and wait for the `SEARCHING` link tag on the display.
2. On your computer or phone, open Bluetooth settings, select **Stick-S3 Macro**
   (or the `bluetooth.name` you configured), and connect. It presents itself as
   a keyboard. The link tag changes to `HANDSHAKE`, then `SECURE` when ready.
3. Focus the application you want to type into and press a button.

**After upgrading from the Deck Clicker (or any earlier build), forget the old
Bluetooth entry on the computer and pair again.** The HID report descriptor was
widened (to support F13-F24) and the device now reports itself as "Stick-S3
Macro"; hosts cache the old descriptor and may ignore keys until re-paired. If it
still misbehaves, also clear the device's bonds (see Bond reset).

Pairing information is retained across restarts, and the active profile is
remembered across power cycles. The device advertises again after a disconnect so
the host can reconnect. Bluetooth stays awake; there is no automatic sleep.

## Controls

The buttons are the two application buttons: **A** (`main`, BtnA) and **B**
(`secondary`, BtnB). The separate reset/power button
is not a macro control: press it once to power on/reset, twice to power off, or
hold it to enter download mode.

| Gesture | What it does |
| --- | --- |
| Tap | Fires on release. |
| Double-tap | Two presses within the double-click window (`double_click_ms`). |
| Hold | Fires **once** when `long_press_ms` is reached, while still held. Releasing afterwards does nothing. |
| A + B together | Opens the profile picker (see below). |

Gesture details:

- A long press with no hold macro assigned behaves like an ordinary tap on
  release.
- If a button has no double-tap macro, two quick taps are two separate taps and
  each fires immediately on release. If it has one, the first tap waits for the
  double-click window so it can tell the difference; a recognized double-tap
  sends only the double-tap macro. Leave double-taps unassigned for the most
  responsive buttons.
- Unassigned slots do nothing.
- Input while Bluetooth is not connected and ready is **discarded**; it is never
  queued for later.

### Profile picker

Press **A and B together** to open the picker. It lists your profiles with the
active one highlighted.

- **B**: highlight the next profile (wraps around).
- **A**: select the highlighted profile and return to the home screen. The choice
  is saved and restored after a power cycle.
- **A + B again**: cancel and keep the current profile.
- 10 seconds without a press cancels.

The picker works even while Bluetooth is disconnected. Double-tap and hold are
disabled inside the picker so taps are immediate. A chord cancels any pending
single or double gesture, so opening the picker never fires a macro.

### Abort a running macro

Macros run keystroke by keystroke, so a long text macro takes a moment. **Press
any button during playback to abort it.** The current key is released and the
rest of the macro is dropped, not replayed. Playback is also aborted, never
resumed, if Bluetooth disconnects, and a macro cannot start unless the link is
ready. Because any press aborts, tapping a single-only slot twice quickly while
its long text macro is still typing stops that macro rather than queueing a
second run.

### Bond reset

Hold **both application buttons during startup for three seconds** and follow the
on-screen progress to clear saved Bluetooth bonds. Then forget the device in your
computer's Bluetooth settings and pair again. This startup gesture does not use
the separate reset/power button.

## On-screen display

The 240x135 display uses a charcoal, ivory, and safety-orange field-manual look
to match the web flasher.

- **Boot:** a brief start-up readout while Bluetooth begins advertising.
- **HOME:** a header with the profile number, profile name, link tag
  (`SEARCHING`, `HANDSHAKE`, `SECURE`) and battery; below it a grid of rows
  `A`/`B` and columns `TAP`/`DBL`/`HOLD` showing each slot's label. An orange
  square marks an assigned slot; an unassigned slot shows a dash.
- **PICKER:** `SELECT PROFILE`, the profile list with a caret on the highlight,
  `ACTIVE` on the current profile, and the hints for B and A.
- **TRANSMITTING:** the macro label, a progress bar, and "press any button to
  abort".
- **Bond reset** and **wrong board** warning screens.

Rendering was checked at build time only; legibility on the real display is a
pending physical check (see the checklist).

## Customize with YAML

Edit `config.yaml`, rebuild, and flash. Configuration is compiled into firmware;
there is no runtime configuration upload, no phone app, and no network service.
The build validates the file and generates `include/generated_config.h`, which
must not be edited or committed. Invalid input fails the build with the path of
the problem.

### Top-level sections

```yaml
bluetooth:
  name: Stick-S3 Macro     # 1-29 UTF-8 bytes
display:
  brightness: 64           # 1-255
gestures:
  debounce_ms: 25          # 1-100
  double_click_ms: 250     # 50-1000
  long_press_ms: 650       # 100-10000
typing:
  keystroke_ms: 15         # 5-100
profiles:                  # ordered list, 1-9 profiles
  - name: ...
```

All sections are required. Unknown or missing fields, and duplicate YAML keys,
are errors. Gesture timing must satisfy
`debounce_ms < double_click_ms < long_press_ms`.

**Typing speed (`typing.keystroke_ms`).** Every keystroke is held for this many
milliseconds, released explicitly, then followed by the same gap before the next.
So a character costs about `2 x keystroke_ms`: at the default 15 ms, roughly 33
characters per second, and a 200-character prompt takes about 6 seconds. Lower
is faster but some hosts and Bluetooth stacks drop or reorder characters; higher
is slower but more reliable. If characters go missing, raise it (try 20-30).
Whether 15 ms is reliable for your longest macro on your host is a physical
check; test it.

### Profiles and limits

```yaml
profiles:
  - name: CLAUDE            # 1-12 printable ASCII, unique ignoring case
    buttons:
      main:                 # button A
        single: <action>
        double: <action>
        long: <action>
      secondary:            # button B
        single: <action>
        double: <action>
        long: <action>
```

- 1 to 9 profiles, each with exactly two buttons (`main`, `secondary`) and
  exactly three gestures (`single`, `double`, `long`): 6 slots per profile. Use
  `null` for a slot you do not want.
- At most 4096 strokes in one macro and 32768 strokes across all profiles. A
  stroke is one key press or one delay step; a typed capital letter or symbol is
  one stroke.
- If you flash a build with fewer profiles than the remembered index, the device
  falls back to the first profile.

### Actions

Each slot takes one of these forms:

```yaml
single: null                                  # unassigned

single: "CMD+SHIFT+4"                         # combo shorthand (a bare string)

single: { keys: "CTRL+C", label: COPY }       # combo with a display label

single:                                       # typed text
  label: REVIEW
  text: "Review the current diff for bugs.\n"   # \n presses ENTER

single:                                       # sequence of steps
  label: SHIP
  sequence:
    - text: "git add -A && git commit -m wip"
    - keys: ENTER
    - { delay: 150 }                          # pause 150 ms
    - text: "git push"
    - keys: ENTER
```

- **Combo** (`keys`, or a bare string): modifiers joined with `+`, ending in at
  most one key, e.g. `CTRL+SHIFT+T`, `ESC`, `F13`. A modifier-only combo such as
  `CMD` or `CTRL+ALT` is allowed. Names are case-insensitive. Quote combos
  containing `+` to keep YAML happy. A bare digit key (`5`) works too.
- **Text:** typed using a **US keyboard layout**. Allowed characters are printable
  ASCII (space through `~`), `\n` (sent as ENTER), and `\t` (sent as TAB). Anything
  else, including accented letters, emoji, and curly quotes, is a build error
  naming the character and its index. In YAML double quotes, write `\n` and `\t`
  as shown; in a block scalar they become literal newlines, which also send ENTER.
- **Sequence:** an ordered, non-empty list of steps. Each step is exactly one of
  `{keys: ...}` (or a bare combo string), `{text: ...}`, or `{delay: ms}`.
  `delay` is 1-10000 ms and is allowed **only inside a sequence**. Use it to give
  an application time to react, such as opening a prompt before typing into it.
- Text is typed as written; there is no key repeat, no media/consumer keys and no
  mouse action.

### Modifiers and aliases

| Modifier | Aliases |
| --- | --- |
| `CTRL` | `CONTROL` |
| `SHIFT` | |
| `ALT` | `OPT`, `OPTION` |
| `CMD` | `GUI`, `WIN`, `META`, `SUPER` |

These all send the **left-hand** modifier. `CMD` is the Command key on a Mac and
the Windows key on Windows. Modifiers must come before the key, and each may
appear once.

### Key names

- Letters `A`-Z and digits `0`-`9`
- Function keys `F1`-`F24`
- `ENTER`, `ESC` (or `ESCAPE`), `BACKSPACE`, `TAB`, `SPACE`
- `DELETE`, `INSERT`, `HOME`, `END`, `PAGE_UP`, `PAGE_DOWN`
- `UP`, `DOWN`, `LEFT`, `RIGHT`
- `CAPS_LOCK`, `PRINT_SCREEN`
- Punctuation: `MINUS`, `EQUAL`, `LBRACKET`, `RBRACKET`, `BACKSLASH`,
  `SEMICOLON`, `QUOTE`, `GRAVE`, `COMMA`, `PERIOD`, `SLASH`

### Labels

`label` is optional, 1-10 printable ASCII characters, and is shown uppercase in
the grid. When omitted:

- a combo uses its normalized text (`cmd+shift+z` becomes `CMD+SHIFT+Z`);
- a text macro uses the first line of the text, uppercased;
- a sequence uses `MACRO`.

A default label longer than 10 characters is truncated to 9 characters plus `~`
(for example `CMD+SHIFT+4` shows as `CMD+SHIFT~`). Set a short explicit label for
anything the default does not describe well.

### Validation errors

Errors name the setting and its location. Examples:

```text
Configuration error: Invalid config.yaml: profiles[1].buttons.main.double.sequence[2].delay must be an integer between 1 and 10000
Configuration error: Invalid config.yaml: profiles[0].buttons.main.single.text: unsupported character '“' at index 7; use printable US ASCII, newline, or tab
Configuration error: Invalid config.yaml: profiles[2].buttons.secondary.long.keys: unsupported key 'ENTR' in 'ENTR'
Configuration error: Invalid config.yaml: profiles[3].name 'deck' duplicates another profile name
Configuration error: Invalid config.yaml: typing.keystroke_ms must be an integer between 5 and 100
Configuration error: Invalid config.yaml: Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms
```

Fix `config.yaml` and rebuild. Do not edit the generated header to bypass
validation.

### Default profiles

| # | Profile | A tap | A double | A hold | B tap | B double | B hold |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `CLAUDE` | `REVIEW` prompt + Enter | `TESTS` prompt + Enter | `COMMIT` prompt + Enter | `ESC` (INTERRUPT) | `SHIFT+TAB` (MODE) | `/clear` + Enter (CLEAR) |
| 2 | `MAC EDIT` | `CMD+V` PASTE | `CMD+C` COPY | `CMD+X` CUT | `CMD+Z` UNDO | `CMD+SHIFT+Z` REDO | `CMD+A` ALL |
| 3 | `WIN EDIT` | `CTRL+V` PASTE | `CTRL+C` COPY | `CTRL+X` CUT | `CTRL+Z` UNDO | `CTRL+Y` REDO | `CTRL+A` ALL |
| 4 | `DECK` | `RIGHT` NEXT | unassigned | unassigned | `LEFT` BACK | unassigned | `ESC` EXIT |

The `CLAUDE` prompts are typed text followed by Enter. They are ordinary
examples in `config.yaml`; edit them to taste. See `config.yaml` for the exact
wording and a comment reference for every action form.

## Tests

Run the configuration and packaging tests (Python `unittest`). Run a firmware
build first: the packaging tests use the pinned esptool image parser that
PlatformIO installs with the build tools.

```sh
.venv/bin/python -m unittest discover -s tests -v          # Windows: .venv\Scripts\python
```

The hardware-independent logic has four native C++ test programs: gesture,
controller, macro player (C++11), and the BLE keyboard transport (C++17, using
fake Arduino/NimBLE headers in `tests/ble`):

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_gesture.cpp -o /tmp/sticks3-gesture-tests && /tmp/sticks3-gesture-tests
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_controller.cpp -o /tmp/sticks3-controller-tests && /tmp/sticks3-controller-tests
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_player.cpp -o /tmp/sticks3-player-tests && /tmp/sticks3-player-tests
c++ -std=c++17 -Wall -Wextra -Werror -Itests/ble -Iinclude tests/test_ble.cpp src/BleKeyboard.cpp -o /tmp/sticks3-ble-tests && /tmp/sticks3-ble-tests
```

No C++ compiler (for example on Windows)? Run them in WSL, or in a throwaway
Docker container from the repository root:

```sh
docker run --rm -v "${PWD}:/src" -w /src gcc:14 sh -c "
  c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_gesture.cpp -o /tmp/g && /tmp/g &&
  c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_controller.cpp -o /tmp/c && /tmp/c &&
  c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/test_player.cpp -o /tmp/p && /tmp/p &&
  c++ -std=c++17 -Wall -Wextra -Werror -Itests/ble -Iinclude tests/test_ble.cpp src/BleKeyboard.cpp -o /tmp/b && /tmp/b"
```

(`${PWD}` is PowerShell/Bash syntax; use `%cd%` in cmd.exe.) The web flasher has
its own checks, described below: `npm ci`, `npm run check`, `npm test`, and
`npm run build` in `web/`.

## Troubleshooting and recovery

- **No serial port:** try another data cable or USB port and rerun the device
  list. Confirm this is a Stick-S3 rather than an older Stick model.
- **Upload cannot connect:** close any serial monitor, reconnect the device, and
  confirm the new port. To enter download mode, connect USB and hold the side
  reset button until the internal green LED flashes, as described in the
  [official Stick-S3 instructions](https://docs.m5stack.com/en/core/StickS3).
  Reidentify the port and rerun the upload command. An application failure can
  usually be recovered by flashing from download mode.
- **Host cannot reconnect, or keys are ignored after upgrading:** check the
  display and Bluetooth settings. Forget the device on the host and pair again;
  if necessary, clear bonds with the startup gesture first. This is expected
  after upgrading from the Deck Clicker because the HID descriptor changed.
- **Connected but nothing happens:** wait for the `SECURE` tag, check which
  application has focus, and make sure the profile has a macro on that slot
  (unassigned slots show a dash). Test in a plain text editor to separate
  keyboard transport from the app's own shortcuts.
- **Text types the wrong characters (for example `@` and `"` swapped, or `y`/`z`
  swapped):** text macros assume a US keyboard layout. Set the host's keyboard
  layout to US while using the device. Layout-dependent shortcuts are affected
  the same way.
- **Characters are dropped or repeated in long text:** raise
  `typing.keystroke_ms` (try 20-30) and reflash. Slower hosts or busy Bluetooth
  links may need more.
- **A macro stops partway:** pressing any button aborts playback, and a Bluetooth
  disconnect aborts it as well. Aborted macros are never resumed or replayed.
- **Configuration fails to build:** read the validation error, fix
  `config.yaml`, and rebuild. Do not edit the generated header.

## Known limitations

- US keyboard layout only for text macros; set the host layout to US.
- Typed text is limited to printable ASCII, newline and tab; no Unicode input.
- No media or consumer keys (volume, play/pause, brightness), and no mouse input.
- Configuration is compile-time only: change `config.yaml`, rebuild and reflash.
  There is no runtime editing, app, or network service.
- At most 9 profiles and 6 slots per profile (2 buttons x tap/double/hold).
- Typing speed is bounded by `typing.keystroke_ms`; long text takes seconds, and
  reliability at low settings depends on the host.
- Macros cannot be paused, resumed, or replayed; only aborted.
- Battery runtime has not been measured and is not claimed.

## Acceptance checklist

Software verification runs the Python configuration/packaging tests, the four
native C++ suites, the firmware build, and the web checks. A passing build does
not prove Bluetooth pairing, button identity, display rendering, typing
reliability, or battery runtime. These physical checks require the device and
are **pending** until you have personally confirmed them:

- [ ] Flash succeeds, the board boots, and the display shows the boot sequence
      and HOME screen with the correct profile and labels.
- [ ] After forgetting the old entry, the device pairs as a keyboard and reaches
      `SECURE`; it reconnects after a device power cycle and after an idle period.
- [ ] Each gesture (tap, double-tap, hold on both buttons) fires the right macro
      exactly once; a hold does not also fire on release.
- [ ] A+B opens the picker; B cycles, A selects, A+B and the 10 s timeout cancel;
      the selected profile survives a power cycle.
- [ ] Pressing a button during a macro aborts it immediately with no stuck keys,
      and nothing is replayed after reconnecting.
- [ ] Input while disconnected is discarded.
- [ ] The longest text macro (such as the `CLAUDE` prompts) types completely and
      correctly at the configured `keystroke_ms` on your host.
- [ ] Display text is legible at the configured brightness, indoors and outdoors.
- [ ] Startup A+B hold clears bonds, and the device pairs again afterward.
- [ ] Battery runtime: **not measured.** Verify for yourself before depending on
      it unplugged.

Keep a regular keyboard available while you validate a new profile.

## Repository layout and web development

Firmware stays at the repository root; `web/` is an independently built Astro
app. There is no workspace manager or runtime server. Reusable Astro components,
central product metadata, CSS theme variables, and a separate TypeScript USB
controller keep the website adaptable to another project.

Install a current Node.js LTS release compatible with the version declared in
`web/package.json` (22.12 or newer), then prepare a local firmware bundle from the
root:

```sh
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/pio run -e sticks3
.venv/bin/python scripts/package_firmware.py --version 0.0.0-dev --output web/public/firmware
npm --prefix web ci
npm --prefix web run dev
```

On Windows, use `.venv\Scripts\python` and `.venv\Scripts\pio`. Open the localhost
URL printed by Astro, including `/stick-s3-macro-clicker/`. Local development
firmware is labeled as a development version. Rebuild and repackage after editing
`config.yaml` so the page and installed firmware agree. Generated firmware
bundles, headers, dependency directories, and site output are ignored by Git.

```sh
npm --prefix web ci
npm --prefix web run check
npm --prefix web test
npm --prefix web run build
npm --prefix web run preview
```

The production build requires a packaged manifest and binaries. Release metadata
and the displayed profiles come from that bundle (which embeds `config.yaml`; see
the security warning above), rather than duplicated settings in the website. The
browser verifies the binary checksums before writing and verifies flash contents
afterward. All installation assets are hosted alongside the site.

## Publish a stable release

The manual **Publish stable firmware and Pages** workflow accepts an existing
stable tag such as `v1.0.0`. It checks out that tag, runs the firmware and website
tests, builds the firmware, generates the bundle, and builds Astro using that
same bundle. GitHub Releases receives the manifest and four binaries; Pages
receives the site with identical firmware files. Visitors install the currently
published stable version; there is no development-build or version selector.
Ordinary pushes and pull requests run the **Validate firmware and web flasher**
workflow, not deployment.

Before the first publication, set the repository's **Settings -> Pages -> Build
and deployment -> Source** to **GitHub Actions**. The site is built with
`BASE_PATH=/stick-s3-macro-clicker/`.

After reviewing and authorizing the changes for publication:

1. Push the reviewed commits and a new stable `vX.Y.Z` tag containing the web
   flasher and workflows.
2. In **Actions**, select **Publish stable firmware and Pages -> Run workflow**
   and enter that existing tag as `version`.
3. Confirm the release and Pages jobs succeed, then check the live site's version,
   firmware downloads, and installation flow.

Published versions cannot be overwritten. Publish a new version for corrections.
If release creation succeeds but Pages deployment fails, rerun only the failed
deployment job; rerunning the full release job intentionally rejects the existing
release. No public release or Git push is performed by local build commands.
Remember that a release publishes `config.yaml` (see the security warning).

## Web-flasher physical acceptance

Software tests do not establish that browser flashing works on a physical device.
The owner will perform these checks; do not mark them complete based on a build:

- [ ] Install from a desktop browser, then confirm boot, display, pairing, and
      the default profiles on a real host.
- [ ] Reflash normally and confirm existing Bluetooth pairing still works.
- [ ] Select clean installation, confirm erase and successful boot, forget the
      computer's old Bluetooth entry, and pair again.
- [ ] Confirm the recovery instructions are usable if download mode or manual
      restart is needed.

See [TODO.md](TODO.md) for implementation and verification progress.
