import { readFileSync } from "node:fs";
import { resolve } from "node:path";

export type Step =
  | string
  | { keys: string }
  | { text: string }
  | { delay: number };

export type Action =
  | null
  | string
  | number
  | { keys: string; label?: string }
  | { text: string; label?: string }
  | { sequence: Step[]; label?: string };

export interface Profile {
  name: string;
  buttons: Record<
    "main" | "secondary",
    Record<"single" | "double" | "long", Action>
  >;
}

export interface Release {
  schemaVersion: number;
  name: string;
  version: string;
  commit: string;
  configSha256: string;
  chipFamily: string;
  flashSize: number | string;
  config: {
    bluetooth: { name: string };
    display: { brightness: number };
    gestures: {
      long_press_ms: number;
      double_click_ms: number;
      debounce_ms: number;
    };
    typing: { keystroke_ms: number };
    profiles: Profile[];
  };
  parts: { path: string; offset: number; size: number; sha256: string }[];
}

export function readRelease(): Release | null {
  try {
    return JSON.parse(
      readFileSync(
        resolve(process.cwd(), "public/firmware/manifest.json"),
        "utf8",
      ),
    );
  } catch {
    return null;
  }
}

const PREVIEW_CHARS = 60;

function truncate(text: string, max: number): string {
  const flat = text.replaceAll("\n", "↵").replaceAll("\t", "⇥");
  return flat.length > max ? `${flat.slice(0, max - 1)}…` : flat;
}

/** Text macros may end in a newline, which the firmware sends as ENTER. */
function describeText(text: string): string {
  const trimmed = text.endsWith("\n") ? text.slice(0, -1) : text;
  const enter = trimmed !== text ? " + ENTER" : "";
  return `Types “${truncate(trimmed, PREVIEW_CHARS)}”${enter}`;
}

function isObject(action: Action): action is Exclude<Action, null | string | number> {
  return typeof action === "object" && action !== null;
}

/** Short on-device style label for a slot. */
export function actionLabel(action: Action): string {
  if (action === null || action === undefined) return "Unassigned";
  if (typeof action === "string") return action.toUpperCase();
  if (typeof action === "number") return String(action);
  if (action.label) return action.label.toUpperCase();
  if ("keys" in action) return action.keys.toUpperCase();
  if ("text" in action) {
    const flat = truncate(action.text.trim(), Infinity);
    if (!flat) return "MACRO";
    return flat.length > 9 ? `${flat.slice(0, 9)}~` : flat;
  }
  return "MACRO";
}

/** Plain-language description of what a slot does. */
export function actionDetail(action: Action): string {
  if (action === null || action === undefined) return "No action";
  if (typeof action === "string") return action.toUpperCase();
  if (typeof action === "number") return String(action);
  if (!isObject(action)) return "No action";
  if ("keys" in action) return action.keys.toUpperCase();
  if ("text" in action) return describeText(action.text);
  const count = action.sequence.length;
  return `Sequence: ${count} step${count === 1 ? "" : "s"}`;
}
