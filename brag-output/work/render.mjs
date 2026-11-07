// Usage: node render.mjs full            -> frames/0000.png ... (660 frames @30fps)
//        node render.mjs stills 1.2 4.5  -> stills/t1.20.png ...
import { createRequire } from "module";
import path from "path";
import fs from "fs";
const require = createRequire(import.meta.url);
const { chromium } = require("/home/alec/.npm/_npx/e41f203b7505f1fb/node_modules/playwright");

const here = path.dirname(new URL(import.meta.url).pathname);
const [mode, ...rest] = process.argv.slice(2);
const FPS = 30, DUR = 22;

const browser = await chromium.launch({ args: ["--allow-file-access-from-files", "--font-render-hinting=none"] });
const page = await browser.newPage({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 });
page.on("pageerror", (e) => console.error("PAGEERROR", e.message));
await page.goto("file://" + path.join(here, "video.html"));
await page.evaluate(() => window.ready);
const stage = await page.$("#stage");

async function shot(t, file) {
  await page.evaluate((tt) => window.renderAt(tt), t);
  await stage.screenshot({ path: file, type: mode === "full" ? "png" : "png" });
}
if (mode === "full") {
  const dir = path.join(here, "frames");
  fs.mkdirSync(dir, { recursive: true });
  const N = FPS * DUR;
  const start = Number(rest[0] || 0), end = Number(rest[1] || N);
  for (let i = start; i < end; i++) {
    await shot(i / FPS, path.join(dir, String(i).padStart(4, "0") + ".png"));
    if (i % 60 === 0) console.log("frame", i);
  }
} else {
  const dir = path.join(here, "stills");
  fs.mkdirSync(dir, { recursive: true });
  for (const s of rest) await shot(Number(s), path.join(dir, `t${Number(s).toFixed(2)}.png`));
}
await browser.close();
