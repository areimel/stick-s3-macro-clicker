// Pixel-faithful JS port of src/Ui.cpp (240x135 display) using the real M5GFX Font0 bitmap.
(function () {
  const W = 240, H = 135;
  const C = {
    BG: "#141310", PANEL: "#1c1b17", INK: "#efebe0", DIM: "#a39e90",
    LINE: "#39372f", ACCENT: "#ff5b1a", BLUE: "#2438e8", ALERT: "#e02018",
  };
  const F = window.FONT0;

  function rect(c, x, y, w, h, col) { c.fillStyle = col; c.fillRect(x, y, w, h); }
  function drawRect(c, x, y, w, h, col) {
    rect(c, x, y, w, 1, col); rect(c, x, y + h - 1, w, 1, col);
    rect(c, x, y, 1, h, col); rect(c, x + w - 1, y, 1, h, col);
  }
  function hline(c, x, y, w, col) { rect(c, x, y, w, 1, col); }
  function vline(c, x, y, h, col) { rect(c, x, y, 1, h, col); }
  function glyph(c, x, y, ch, col, s) {
    c.fillStyle = col;
    for (let i = 0; i < 5; i++) {
      const b = F[ch * 5 + i];
      for (let j = 0; j < 8; j++) if (b & (1 << j)) c.fillRect(x + i * s, y + j * s, s, s);
    }
  }
  function textWidth(s, size, sp) { const n = s.length; return n ? n * (6 * size + sp) - sp : 0; }
  function label(c, x, y, s, col, size = 1, sp = 1) {
    for (const ch of s) { glyph(c, x, y, ch.charCodeAt(0), col, size); x += 6 * size + sp; }
  }
  function labelRight(c, r, y, s, col, size = 1, sp = 1) { label(c, r - textWidth(s, size, sp), y, s, col, size, sp); }
  function labelCenter(c, cx, y, s, col, size = 1, sp = 1) { label(c, cx - Math.trunc(textWidth(s, size, sp) / 2), y, s, col, size, sp); }
  function square(c, x, y, s, col) { rect(c, x, y, s, s, col); }
  function tick(c, x, y, dx, dy, col) {
    hline(c, dx > 0 ? x : x - 5, y, 6, col);
    vline(c, x, dy > 0 ? y : y - 5, 6, col);
  }
  function cornerTicks(c, x0, y0, x1, y1, col) {
    tick(c, x0, y0, 1, 1, col); tick(c, x1, y0, -1, 1, col);
    tick(c, x0, y1, 1, -1, col); tick(c, x1, y1, -1, -1, col);
  }
  function battery(c, right, y, pct) {
    const x = right - 18;
    drawRect(c, x, y, 16, 7, C.DIM); rect(c, x + 16, y + 2, 2, 3, C.DIM);
    const w = Math.trunc((pct * 12 + 50) / 100);
    rect(c, x + 2, y + 2, w, 3, C.INK);
    labelRight(c, x - 4, y, pct + "%", C.INK, 1, 0);
  }
  function footer(c, left, right) {
    hline(c, 6, 118, W - 12, C.LINE);
    if (left) label(c, 8, 124, left, C.DIM);
    if (right) labelRight(c, W - 8, 124, right, C.DIM);
  }
  const pad = (n) => String(n).padStart(2, "0");

  // m: {screen, link, battery, profiles, profileIndex, highlighted, macroLabel, sent, total, ms}
  function drawHome(c, m) {
    const p = m.profiles[m.profileIndex];
    square(c, 8, 5, 6, C.ACCENT);
    label(c, 18, 5, `PROFILE ${pad(m.profileIndex + 1)}/${pad(m.profiles.length)}`, C.DIM);
    battery(c, W - 8, 4, m.battery);
    label(c, 8, 17, p.name.toUpperCase(), C.INK, 2, 0);
    const tag = m.link || "SECURE";
    const tw = textWidth(tag, 1, 1) + 12, tx = W - 8 - tw;
    if (tag === "SECURE") { rect(c, tx, 18, tw, 12, C.ACCENT); label(c, tx + 6, 20, tag, C.BG); }
    else {
      const on = Math.floor(m.ms / 500) & 1;
      const col = tag === "HANDSHAKE" ? C.INK : C.DIM;
      drawRect(c, tx, 18, tw, 12, col === C.INK ? C.INK : C.LINE);
      label(c, tx + 6, 20, tag, col);
      if (on) square(c, tx - 7, 22, 4, C.ACCENT);
    }
    hline(c, 6, 35, W - 12, C.LINE);
    const gx = 24, cw = 72, headY = 40, row0 = 51, rh = 32;
    ["TAP", "DBL", "HOLD"].forEach((h, g) => label(c, gx + g * cw + 8, headY, h, C.DIM));
    hline(c, 6, 50, W - 12, C.LINE);
    hline(c, 6, row0 + rh, W - 12, C.LINE);
    for (let g = 0; g < 3; g++) vline(c, gx + g * cw - 2, 37, 80, C.LINE);
    for (let r = 0; r < 2; r++) {
      const y = row0 + r * rh;
      label(c, 10, y + rh / 2 - 8, r ? "B" : "A", C.ACCENT, 2, 0);
      for (let g = 0; g < 3; g++) {
        const x = gx + g * cw, cy = y + rh / 2 - 4;
        const lab = p.slots[r][g];
        const shown = lab && (!m.reveal || m.reveal[r][g]);
        if (shown) { square(c, x + 3, cy + 2, 3, C.ACCENT); label(c, x + 8, cy, lab.toUpperCase().slice(0, 11), C.INK, 1, 0); }
        else hline(c, x + 8, cy + 4, 8, C.LINE);
      }
    }
    cornerTicks(c, 2, 2, W - 3, H - 3, C.LINE);
    footer(c, "A+B  PROFILES", "NO. 0001");
  }
  function drawPicker(c, m) {
    square(c, 8, 5, 6, C.ACCENT);
    label(c, 18, 5, "SELECT PROFILE", C.INK);
    labelRight(c, W - 8, 5, `${m.highlighted + 1}/${m.profiles.length}`, C.DIM);
    hline(c, 6, 16, W - 12, C.LINE);
    const rowH = 19, top = 20;
    m.profiles.forEach((p, idx) => {
      if (idx >= 5) return;
      const y = top + idx * rowH, hi = idx === m.highlighted;
      const fg = hi ? C.BG : C.INK, sub = hi ? C.BG : C.DIM;
      if (hi) rect(c, 6, y, W - 12, rowH - 2, C.ACCENT);
      label(c, 12, y + 5, pad(idx + 1), sub);
      label(c, 34, y + 1, p.name.toUpperCase(), fg, 2, 0);
      if (idx === m.profileIndex) labelRight(c, W - 12, y + 5, "ACTIVE", hi ? C.BG : C.ACCENT);
    });
    footer(c, "B NEXT  A ENGAGE  A+B CANCEL", null);
  }
  function drawTransmit(c, m) {
    const blink = Math.floor(m.ms / 400) & 1;
    rect(c, 0, 0, W, 15, C.ACCENT);
    label(c, 8, 4, "TRANSMITTING", C.BG, 1, 2);
    if (blink) square(c, W - 14, 4, 6, C.BG); else drawRect(c, W - 14, 4, 6, 6, C.BG);
    const lab = m.macroLabel.toUpperCase();
    const size = textWidth(lab, 3, 0) <= W - 16 ? 3 : 2;
    labelCenter(c, W / 2, 36, lab, C.INK, size, 0);
    const bx = 12, by = 78, bw = W - 24, bh = 12;
    drawRect(c, bx, by, bw, bh, C.LINE);
    let fill = m.total ? Math.trunc((m.sent * (bw - 4)) / m.total) : 0;
    rect(c, bx + 2, by + 2, Math.min(fill, bw - 4), bh - 4, C.ACCENT);
    for (let i = 1; i < 4; i++) vline(c, bx + Math.trunc(bw * i / 4), by + bh + 1, 3, C.LINE);
    labelCenter(c, W / 2, 98, `${m.sent} / ${m.total}`, C.INK);
    footer(c, null, null);
    labelCenter(c, W / 2, 124, "PRESS ANY BUTTON TO ABORT", C.DIM);
  }
  // step 0..5 of Ui::boot
  function drawBoot(c, name, s) {
    const lines = ["LINK INIT", "HID KEYBOARD", "PROFILES LOADED", "AWAITING HOST"];
    cornerTicks(c, 2, 2, W - 3, H - 3, C.ACCENT);
    square(c, 12, 12, 6, C.ACCENT);
    label(c, 24, 12, "STICK-S3 / MACRO", C.INK, 1, 2);
    labelRight(c, W - 12, 12, "NO. 0001", C.DIM);
    hline(c, 12, 26, W - 24, C.LINE);
    label(c, 12, 32, name.toUpperCase(), C.DIM);
    for (let i = 0; i < 4 && i < s; i++) {
      const done = i < s - 1 || s === 5;
      label(c, 12, 48 + i * 12, ">", C.ACCENT);
      label(c, 24, 48 + i * 12, lines[i], C.INK);
      labelRight(c, W - 12, 48 + i * 12, done ? "OK" : "..", done ? C.ACCENT : C.DIM);
    }
    drawRect(c, 12, 112, W - 24, 8, C.LINE);
    rect(c, 14, 114, Math.trunc((W - 28) * s / 5), 4, C.ACCENT);
  }
  function render(canvas, m) {
    const c = canvas.getContext("2d");
    rect(c, 0, 0, W, H, C.BG);
    if (m.screen === "HOME") drawHome(c, m);
    else if (m.screen === "PICKER") drawPicker(c, m);
    else if (m.screen === "TRANSMIT") drawTransmit(c, m);
    else if (m.screen === "BOOT") drawBoot(c, "Stick-S3 Macro", m.step);
  }
  // Labels mirrored from config.yaml (null = unassigned).
  const PROFILES = [
    { name: "CLAUDE", slots: [["REVIEW", "TESTS", "COMMIT"], ["INTERRUPT", "MODE", "CLEAR"]] },
    { name: "MAC EDIT", slots: [["PASTE", "COPY", "CUT"], ["UNDO", "REDO", "ALL"]] },
    { name: "WIN EDIT", slots: [["PASTE", "COPY", "CUT"], ["UNDO", "REDO", "ALL"]] },
    { name: "DECK", slots: [["NEXT", null, null], ["BACK", null, "EXIT"]] },
  ];
  window.Screen = { render, PROFILES, W, H, C };
})();
