# /brag-slim — Stick-S3 Macro Clicker launch video

## Context
The user ran `/brag-slim` with no input, in this repo, so the input is the **project**. The deliverable is a short, polished launch video (about 20s, landscape 1920×1080, 30fps) plus a poster and share copy, written to `brag-output/` with all intermediates in `brag-output/work/`. No firmware, web or config files are changed. Defaults apply: tone `default` (punchy, clean), fitted to the project's own field-manual look.

## Inspection answers
- **What it is:** firmware that turns an M5Stack Stick-S3 into a pocket Bluetooth macro keyboard: 2 buttons × tap/double/hold = 6 macros per profile, up to 9 profiles, set in one `config.yaml`, installed from the browser.
- **Who it's for:** people who fire the same shortcuts and prompts all day (the default CLAUDE profile types whole Claude Code prompts).
- **What sets it apart:** one tap types a full prompt (REVIEW = 94 chars at ~33 chars/s, from the README); you switch profiles on the device; no app or network service; flash it from a web page.
- **Funniest real claim:** a two-button stick that types "Review the current diff for bugs, regressions, and missing tests. Report findings by severity." for you.
- **Visual hook:** the blue stick, a button press, and that prompt typing itself out.
- **Real UI shown:** the 240×135 device screens (BOOT, HOME, PICKER, TRANSMITTING), ported pixel-for-pixel from `src/Ui.cpp` to canvas using the **real Font0 bitmap** from `.pio/libdeps/sticks3/M5GFX/src/lgfx/Fonts/glcdfont.h`; the site's FIG.01 plate SVG (`web/src/components/DevicePlate.astro`, reused verbatim); real `config.yaml` text; the site h1 "Your macros. Your signal."; and the orange install button styled from `web/src/styles/theme.css`.
- **Identity:** `--bg #141310`, `--panel #1c1b17`, `--ink #efebe0`, `--dim #a39e90`, `--line #39372f`, `--accent #ff5b1a`, `--blue #2438e8`. The site uses the system monospace stack, so the video uses the locally installed Roboto Mono, falling back to DejaVu Sans Mono. Look: hairlines, corner ticks, small letter-spaced labels.
- **Share caption idea:** "Two buttons, six macros, nine profiles. My Stick-S3 types whole Claude prompts for me now."
- **Privacy:** `config.yaml` is already public (it's embedded in the release manifest). No personal data appears anywhere. The terminal shown in the video is a generic fictional `~/project $` prompt.

## Storyboard: 120 BPM, beat = 0.5s, every cut on a beat, total 21.0s
| # | Time | Scene | On-screen copy (held ≥0.3s/word once settled) |
|---|---|---|---|
| 1 Hook | 0.0–3.0 | The blue stick (body drawn in DevicePlate styling, sized for a landscape 240:135 screen) slides in showing CLAUDE HOME. Its A button presses on beat 2 (ripple and a click sound), the screen flips to TRANSMITTING REVIEW and the progress bar fills. On the right, a terminal types the real REVIEW prompt at ~33 chars/s. | "One tap. A whole prompt." |
| 2 Reveal | 3.0–6.0 | Dip through the background, then the FIG.01 plate SVG draws in by stroke, with the title beside it. | Eyebrow "M5STACK STICK–S3 / BLUETOOTH HID"; title "Stick-S3 Macro Clicker"; sub "A pocket Bluetooth macro keyboard." |
| 3 Gestures | 6.0–10.0 | A large pixel-exact HOME screen (×5 scale). TAP, DBL and HOLD light up one per beat pair, and REVIEW / TESTS / COMMIT pulse with a small press glyph (1, 2, hold bar). | "Tap. Double-tap. Hold." then "2 buttons × 3 gestures = 6 macros" |
| 4 Profiles | 10.0–14.0 | An "A+B" chord glyph opens PICKER. B steps CLAUDE → MAC EDIT → WIN EDIT → DECK on beats, A engages, and HOME shows NEXT / BACK / EXIT. | "A+B switches profiles. Up to 9." |
| 5 YAML | 14.0–17.0 | Left: a `config.yaml` excerpt types in (`- name: DECK` / `single: { keys: RIGHT, label: NEXT }`) with syntax tinting. Right: the device HOME showing that profile. | "Every macro lives in one YAML file." |
| 6 Outro | 17.0–21.0 | The site's h1 settles. The orange "INSTALL FIRMWARE →" button gets a simulated click, and a 3px progress bar fills to "VERIFIED". The URL sits underneath. | "Your macros. Your signal." / "Install from your browser" / `areimel.github.io/stick-s3-macro-clicker` |

Transitions: staggered (old content out, then new content in) or a dip through `--bg`, never a muddy crossfade. Light camera drift keeps every frame alive. Poster candidate: scene 6 settled at ~20.0s, or scene 3 at ~9.5s.

## Sound (Python stdlib only: `wave`/`array`/`math`; numpy is not installed)
A minimal electronic track in A minor at 120 BPM, on one grid counted from frame 0:
- Soft kick on quarters from scene 2 on (the hook keeps a pulse only), closed hats on 8ths, sub-bass on A/F/C/G roots.
- A plucked arpeggio and a quiet pad.
- Effects tuned to the key and placed on the grid: button clicks are short pitched ticks (A5) on the press beats. Typing is very low tick texture quantized to 32nds. Each cut gets a short filtered-noise riser into it, plus one soft low impact at the reveal.
- Mix: effects sit around −12dB under the music, a gentle soft-clip limiter, and a 0.5s fade at the end. Written to `work/audio.wav`.

## Build pipeline (all tools are already on the machine; nothing gets installed)
1. `work/fontdata.js`: extract the Font0 byte array from `glcdfont.h` with a small Python script.
2. `work/video.html`: a single page exposing `window.renderAt(t)`, a pure function of time. It contains:
   - `screen.js`: a JS port of `drawHome`, `drawPicker`, `drawTransmit` and `boot`, plus the label/tick/battery/footer helpers from `src/Ui.cpp`. It draws into a 240×135 canvas, scaled with `image-rendering: pixelated`.
   - The profile data, hand-mirrored from `config.yaml` (labels only).
   - The DevicePlate SVG markup.
   - DOM/CSS scenes driven by eased timeline helpers.
3. `work/render.mjs`: Node 24, importing Playwright 1.64.1 by absolute path from `~/.npm/_npx/e41f203b7505f1fb/node_modules/playwright` with the cached Chromium. It waits for `document.fonts.ready`, then for each of 630 frames calls `renderAt(i/30)` and screenshots to `work/frames/%04d.png`.
4. QA before the full render: capture stills of every scene at its settled point and mid-transition, then view them. Fix overflow, collisions and contrast.
5. Encode with ffmpeg: `libx264 -crf 18 -pix_fmt yuv420p` plus AAC audio, giving `brag.mp4` (21.0s).
6. Poster: export the strongest settled frame to `brag.jpg`, then replace frame 0 with it (an overlay with `enable='eq(n,0)'`) so the duration and sync stay the same.

## Deliverables (`brag-output/`)
`brag.mp4`, `brag.jpg`, `brag-plan.md` (this storyboard), `share-copy.txt` (1–3 sentences, no "excited to share"), and `work/`. Nothing is committed (it isn't requested); `brag-output/` stays untracked.

## Verification
- `ffprobe` shows 1920×1080, 30fps, about 21.0s, and an audio stream of the same length.
- Pull stills at each scene midpoint, each cut ±0.2s, and frame 0, then visually inspect them.
- Check the audio peak level is ≤ −1dBFS.
- Tell the user where the files are and the creative angle in one sentence, and offer to re-roll a scene or try another tone.

## As built
- Final length is 22.0s, not 21s. The hook runs 0–4.0s, so the real 95-stroke REVIEW macro finishes typing at true speed (15 ms keys, so about 30 ms per stroke, ending at 3.4s). Every later scene shifts by +1s: reveal 4–7, gestures 7–11, profiles 11–15, YAML 15–18, outro 18–22.
- Scene 5 shows the real boot sequence (`Ui::boot`) after BUILD → FLASH, rather than a live edit, because configuration is compile-time only.
- Poster is frame 627 (20.9s, install verified), baked in as frame 0.
- Audio: −12.7 LUFS integrated, −1.6 dBFS peak.
