"""Validate config.yaml and compile it into the firmware's generated header.

The header contains ready-to-send HID strokes, so firmware never parses key
names, text, or labels.
"""

import argparse
import json
from pathlib import Path

try:
    import yaml
except ImportError as exc:
    raise SystemExit(
        "Missing PyYAML. Run: python -m pip install -r requirements.txt "
        "using the Python environment that runs PlatformIO."
    ) from exc


class UniqueKeyLoader(yaml.SafeLoader):
    pass


def _mapping(loader, node, deep=False):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if not isinstance(key, str):
            raise ValueError("YAML mapping keys must be strings")
        if key in result:
            raise ValueError(f"Duplicate YAML field: {key}")
        result[key] = loader.construct_object(value_node, deep=deep)
    return result


UniqueKeyLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _mapping)

MAX_PROFILES = 9
MAX_PROFILE_NAME = 12
MAX_LABEL = 10
MAX_STROKES_PER_MACRO = 4096
MAX_STROKES_TOTAL = 32768

BUTTONS = ("main", "secondary")
GESTURES = ("single", "double", "long")

MODIFIERS = {
    "CTRL": 0x01, "CONTROL": 0x01,
    "SHIFT": 0x02,
    "ALT": 0x04, "OPT": 0x04, "OPTION": 0x04,
    "CMD": 0x08, "GUI": 0x08, "WIN": 0x08, "META": 0x08, "SUPER": 0x08,
}
SHIFT = MODIFIERS["SHIFT"]
ENTER, TAB = 0x28, 0x2B

KEYS = {chr(ord("A") + n): 0x04 + n for n in range(26)}
KEYS.update({str(n): 0x1D + n for n in range(1, 10)})
KEYS["0"] = 0x27
KEYS.update({f"F{n}": 0x39 + n for n in range(1, 13)})
KEYS.update({f"F{n}": 0x68 + n - 13 for n in range(13, 25)})
KEYS.update({
    "ENTER": 0x28, "ESC": 0x29, "ESCAPE": 0x29,
    "BACKSPACE": 0x2A, "TAB": 0x2B, "SPACE": 0x2C,
    "CAPS_LOCK": 0x39, "PRINT_SCREEN": 0x46, "INSERT": 0x49, "DELETE": 0x4C,
    "HOME": 0x4A, "PAGE_UP": 0x4B, "END": 0x4D, "PAGE_DOWN": 0x4E,
    "RIGHT": 0x4F, "LEFT": 0x50, "DOWN": 0x51, "UP": 0x52,
    "MINUS": 0x2D, "EQUAL": 0x2E, "LBRACKET": 0x2F, "RBRACKET": 0x30,
    "BACKSLASH": 0x31, "SEMICOLON": 0x33, "QUOTE": 0x34, "GRAVE": 0x35,
    "COMMA": 0x36, "PERIOD": 0x37, "SLASH": 0x38,
})


def _build_ascii():
    """US keyboard layout: character -> (modifiers, usage) for 0x20-0x7E."""
    table = {chr(ord("a") + n): (0, 0x04 + n) for n in range(26)}
    table.update({chr(ord("A") + n): (SHIFT, 0x04 + n) for n in range(26)})
    table.update({str(n): (0, 0x1D + n) for n in range(1, 10)})
    table["0"] = (0, 0x27)
    table.update({c: (SHIFT, 0x1E + n) for n, c in enumerate("!@#$%^&*(")})
    table[")"] = (SHIFT, 0x27)
    table[" "] = (0, 0x2C)
    plain = "-=[]\\;'`,./"
    shifted = '_+{}|:"~<>?'
    usages = [0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38]
    for p, s, u in zip(plain, shifted, usages):
        table[p] = (0, u)
        table[s] = (SHIFT, u)
    return table


ASCII = _build_ascii()
ASCII["\n"] = (0, ENTER)
ASCII["\t"] = (0, TAB)


def fields(value, expected, location):
    if not isinstance(value, dict):
        raise ValueError(f"{location} must be a mapping")
    missing, extra = set(expected) - value.keys(), value.keys() - set(expected)
    if missing or extra:
        raise ValueError(f"{location}: missing fields {sorted(missing)}; unknown fields {sorted(extra)}")
    return value


def integer(value, low, high, location):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{location} must be an integer between {low} and {high}")
    return value


def is_printable(text):
    return all(0x20 <= ord(c) <= 0x7E for c in text)


def parse_combo(value, location):
    """Return (modifiers, usage, normalized) for a combo like "CMD+SHIFT+Z"."""
    # YAML naturally parses bare digit keys as integers. Accept these as keys.
    if type(value) is int and 0 <= value <= 9:
        value = str(value)
    if not isinstance(value, str):
        raise ValueError(f"{location}: key combo must be a string such as \"CTRL+C\"")
    parts = [part.strip().upper() for part in value.split("+")]
    mods, usage, seen = 0, 0, set()
    for index, part in enumerate(parts):
        if not part:
            raise ValueError(f"{location}: empty key name in {value!r}")
        if part in seen:
            raise ValueError(f"{location}: duplicate {part} in {value!r}")
        seen.add(part)
        if part in MODIFIERS:
            if usage:
                raise ValueError(f"{location}: modifier {part} must come before the key in {value!r}")
            if mods & MODIFIERS[part]:
                raise ValueError(f"{location}: duplicate modifier {part} in {value!r}")
            mods |= MODIFIERS[part]
        elif part in KEYS:
            if index != len(parts) - 1:
                raise ValueError(f"{location}: key {part} must be last in {value!r}; only one non-modifier key is allowed")
            usage = KEYS[part]
        else:
            raise ValueError(f"{location}: unsupported key {part!r} in {value!r}")
    return mods, usage, "+".join(parts)


def encode_text(text, location):
    if not isinstance(text, str) or not text:
        raise ValueError(f"{location} must be a non-empty string")
    strokes = []
    for index, char in enumerate(text):
        if char not in ASCII:
            raise ValueError(
                f"{location}: unsupported character {char!r} at index {index}; "
                "use printable US ASCII, newline, or tab"
            )
        mods, usage = ASCII[char]
        strokes.append((mods, usage, 0))
    return strokes


def shorten(text):
    return text if len(text) <= MAX_LABEL else text[:MAX_LABEL - 1] + "~"


def parse_label(value, location):
    if (not isinstance(value, str) or not 1 <= len(value) <= MAX_LABEL
            or not is_printable(value) or not value.strip()):
        raise ValueError(f"{location} must be 1–{MAX_LABEL} printable ASCII characters")
    return value.upper()


def text_label(text):
    line = text.split("\n", 1)[0]
    line = "".join(c if 0x20 <= ord(c) <= 0x7E else " " for c in line).strip()
    return shorten(line.upper()) if line else "MACRO"


def parse_step(step, location):
    if isinstance(step, dict):
        kind = [key for key in ("keys", "text", "delay") if key in step]
        if len(kind) != 1 or len(step) != 1:
            raise ValueError(f"{location} must contain exactly one of keys, text, or delay")
        kind = kind[0]
        if kind == "keys":
            mods, usage, _ = parse_combo(step["keys"], f"{location}.keys")
            return [(mods, usage, 0)]
        if kind == "text":
            return encode_text(step["text"], f"{location}.text")
        return [(0, 0, integer(step["delay"], 1, 10000, f"{location}.delay"))]
    mods, usage, _ = parse_combo(step, location)
    return [(mods, usage, 0)]


def parse_action(value, location):
    """Return None for an unassigned slot, otherwise (label, strokes)."""
    if value is None:
        return None
    if isinstance(value, dict):
        kind = [key for key in ("keys", "text", "sequence") if key in value]
        unknown = set(value) - {"keys", "text", "sequence", "label"}
        if len(kind) != 1 or unknown:
            raise ValueError(
                f"{location} must contain exactly one of keys, text, or sequence, "
                f"plus an optional label (unknown fields {sorted(unknown)})"
            )
        kind = kind[0]
        if kind == "keys":
            mods, usage, normalized = parse_combo(value["keys"], f"{location}.keys")
            strokes, label = [(mods, usage, 0)], shorten(normalized)
        elif kind == "text":
            strokes = encode_text(value["text"], f"{location}.text")
            label = text_label(value["text"])
        else:
            steps = value["sequence"]
            if not isinstance(steps, list) or not steps:
                raise ValueError(f"{location}.sequence must be a non-empty list")
            strokes = []
            for index, step in enumerate(steps):
                strokes += parse_step(step, f"{location}.sequence[{index}]")
            label = "MACRO"
        if "label" in value:
            label = parse_label(value["label"], f"{location}.label")
    else:
        mods, usage, normalized = parse_combo(value, location)
        strokes, label = [(mods, usage, 0)], shorten(normalized)
    if len(strokes) > MAX_STROKES_PER_MACRO:
        raise ValueError(f"{location} compiles to {len(strokes)} strokes; the limit is {MAX_STROKES_PER_MACRO}")
    return label, strokes


def parse_profiles(profiles):
    if not isinstance(profiles, list) or not 1 <= len(profiles) <= MAX_PROFILES:
        raise ValueError(f"profiles must be a list of 1–{MAX_PROFILES} profiles")
    result, names = [], set()
    for p, profile in enumerate(profiles):
        where = f"profiles[{p}]"
        profile = fields(profile, ["name", "buttons"], where)
        name = profile["name"]
        if not isinstance(name, str) or not is_printable(name) or not 1 <= len(name.strip()) <= MAX_PROFILE_NAME:
            raise ValueError(f"{where}.name must be 1–{MAX_PROFILE_NAME} printable ASCII characters")
        name = name.strip()
        if name.lower() in names:
            raise ValueError(f"{where}.name {name!r} duplicates another profile name")
        names.add(name.lower())
        buttons = fields(profile["buttons"], BUTTONS, f"{where}.buttons")
        slots = {}
        for button in BUTTONS:
            mapping = fields(buttons[button], GESTURES, f"{where}.buttons.{button}")
            for gesture in GESTURES:
                slots[(button, gesture)] = parse_action(
                    mapping[gesture], f"{where}.buttons.{button}.{gesture}")
        result.append((name, slots))
    return result


def cstr(text):
    # json.dumps gives C-compatible escapes; escape '?' so trigraphs cannot form.
    return json.dumps(text, ensure_ascii=False).replace("?", "\\?")


def stroke_array(name, strokes):
    cells = [f"{{0x{m:02X}, 0x{u:02X}, {d}}}" for m, u, d in strokes]
    rows = [", ".join(cells[i:i + 4]) for i in range(0, len(cells), 4)]
    return f"static const Stroke {name}[] = {{\n  " + ",\n  ".join(rows) + "\n};"


def render(data):
    fields(data, ["bluetooth", "display", "gestures", "typing", "profiles"], "config")
    bluetooth = fields(data["bluetooth"], ["name"], "bluetooth")
    name = bluetooth["name"]
    if not isinstance(name, str) or not name.strip() or len(name.encode("utf-8")) > 29:
        raise ValueError("bluetooth.name must contain 1–29 UTF-8 bytes")
    if any(ord(c) < 32 or ord(c) == 127 for c in name):
        raise ValueError("bluetooth.name must not contain control characters")
    display = fields(data["display"], ["brightness"], "display")
    brightness = integer(display["brightness"], 1, 255, "display.brightness")
    timing = fields(data["gestures"], ["debounce_ms", "double_click_ms", "long_press_ms"], "gestures")
    debounce = integer(timing["debounce_ms"], 1, 100, "gestures.debounce_ms")
    double = integer(timing["double_click_ms"], 50, 1000, "gestures.double_click_ms")
    hold = integer(timing["long_press_ms"], 100, 10000, "gestures.long_press_ms")
    if not debounce < double < hold:
        raise ValueError("Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms")
    typing = fields(data["typing"], ["keystroke_ms"], "typing")
    keystroke = integer(typing["keystroke_ms"], 5, 100, "typing.keystroke_ms")
    profiles = parse_profiles(data["profiles"])

    total = sum(len(s[1]) for _, slots in profiles for s in slots.values() if s)
    if total > MAX_STROKES_TOTAL:
        raise ValueError(f"profiles compile to {total} strokes in total; the limit is {MAX_STROKES_TOTAL}")

    lines = [
        "// Generated from config.yaml. Edit YAML, not this file.",
        "#pragma once", "#include <stdint.h>", '#include "Macro.h"', "",
        "namespace MacroConfig {",
        f"constexpr const char* DEVICE_NAME = {cstr(name)};",
        f"constexpr uint8_t BRIGHTNESS = {brightness};",
        f"constexpr uint32_t DEBOUNCE_MS = {debounce};",
        f"constexpr uint32_t DOUBLE_CLICK_MS = {double};",
        f"constexpr uint32_t LONG_PRESS_MS = {hold};",
        f"constexpr uint32_t KEYSTROKE_MS = {keystroke};",
        "",
    ]
    entries = []
    for p, (profile_name, slots) in enumerate(profiles):
        rows = []
        for button in BUTTONS:
            cells = []
            for gesture in GESTURES:
                slot = slots[(button, gesture)]
                if slot is None:
                    cells.append('{"", nullptr, 0}')
                    continue
                label, strokes = slot
                array = f"P{p}_{button.upper()}_{gesture.upper()}"
                lines += [stroke_array(array, strokes), ""]
                cells.append(f"{{{cstr(label)}, {array}, {len(strokes)}}}")
            rows.append("{ " + ", ".join(cells) + " }")
        entries.append(f"  {{{cstr(profile_name)}, {{\n    " + ",\n    ".join(rows) + "\n  }}")
    lines += [
        f"constexpr uint8_t PROFILE_COUNT = {len(profiles)};",
        "static const Profile PROFILES[PROFILE_COUNT] = {",
        ",\n".join(entries),
        "};",
        "} // namespace MacroConfig", "",
    ]
    return "\n".join(lines)


def generate(source, destination):
    try:
        output = render(yaml.load(Path(source).read_text(encoding="utf-8"), Loader=UniqueKeyLoader))
    except (ValueError, yaml.YAMLError) as exc:
        raise ValueError(f"Invalid {source}: {exc}") from exc
    destination = Path(destination)
    if not destination.exists() or destination.read_text(encoding="utf-8") != output:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="config.yaml")
    parser.add_argument("--output", default="include/generated_config.h")
    args = parser.parse_args()
    try:
        generate(args.config, args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"Configuration error: {exc}\n")
