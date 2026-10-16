import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generate_config", ROOT / "scripts/generate_config.py")
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)

CTRL, SHIFT, ALT, GUI = 0x01, 0x02, 0x04, 0x08


def profile(name="P", **slots):
    """Build a profile; slots like main_single=... default to None."""
    buttons = {b: {g: slots.get(f"{b}_{g}") for g in ("single", "double", "long")}
               for b in ("main", "secondary")}
    return {"name": name, "buttons": buttons}


class ConfigTests(unittest.TestCase):
    def setUp(self):
        self.data = config.yaml.load((ROOT / "config.yaml").read_text(encoding="utf-8"),
                                     Loader=config.UniqueKeyLoader)

    def render_action(self, action):
        """Render a one-profile config and return (label, strokes) of main.single."""
        self.data["profiles"] = [profile(main_single=action)]
        return config.parse_profiles(self.data["profiles"])[0][1][("main", "single")]

    def assertRejected(self, action, fragment=None):
        with self.assertRaises(ValueError) as caught:
            self.render_action(action)
        if fragment:
            self.assertIn(fragment, str(caught.exception))

    # --- defaults ---------------------------------------------------------
    def test_defaults_render(self):
        result = config.render(self.data)
        self.assertTrue(result.startswith("// Generated from config.yaml. Edit YAML, not this file."))
        self.assertIn("#pragma once", result)
        self.assertIn('#include "Macro.h"', result)
        self.assertIn("namespace MacroConfig {", result)
        self.assertIn('DEVICE_NAME = "Stick-S3 Macro"', result)
        self.assertIn("KEYSTROKE_MS = 15;", result)
        self.assertIn("PROFILE_COUNT = 4;", result)
        self.assertIn("static const Stroke P0_MAIN_SINGLE[] = {", result)
        self.assertIn('{"", nullptr, 0}', result)
        self.assertNotIn("P3_MAIN_DOUBLE", result)

    def test_default_profiles(self):
        names = [n for n, _ in config.parse_profiles(self.data["profiles"])]
        self.assertEqual(names, ["CLAUDE", "MAC EDIT", "WIN EDIT", "DECK"])
        deck = config.parse_profiles(self.data["profiles"])[3][1]
        self.assertEqual(deck[("main", "single")], ("NEXT", [(0, 0x4F, 0)]))
        self.assertEqual(deck[("secondary", "long")], ("EXIT", [(0, 0x29, 0)]))
        self.assertIsNone(deck[("main", "double")])
        mac = config.parse_profiles(self.data["profiles"])[1][1]
        self.assertEqual(mac[("secondary", "double")], ("REDO", [(GUI | SHIFT, 0x1D, 0)]))
        win = config.parse_profiles(self.data["profiles"])[2][1]
        self.assertEqual(win[("secondary", "double")], ("REDO", [(CTRL, 0x1C, 0)]))
        claude = config.parse_profiles(self.data["profiles"])[0][1]
        label, strokes = claude[("secondary", "long")]
        self.assertEqual(label, "CLEAR")
        self.assertEqual(strokes[-1], (0, config.ENTER, 0))
        self.assertEqual(len(strokes), 7)

    # --- combos -----------------------------------------------------------
    def test_combo_parsing(self):
        cases = {
            "A": (0, 0x04, "A"),
            "ctrl+c": (CTRL, 0x06, "CTRL+C"),
            " Cmd + Shift + z ": (GUI | SHIFT, 0x1D, "CMD+SHIFT+Z"),
            "CONTROL+ALT+DELETE": (CTRL | ALT, 0x4C, "CONTROL+ALT+DELETE"),
            "opt+option_": None,
            "OPTION+X": (ALT, 0x1B, "OPTION+X"),
            "WIN+E": (GUI, 0x08, "WIN+E"),
            "META+SUPER": None,
            "SHIFT": (SHIFT, 0, "SHIFT"),
            "CTRL+SHIFT": (CTRL | SHIFT, 0, "CTRL+SHIFT"),
            "F13": (0, 0x68, "F13"),
            "F24": (0, 0x73, "F24"),
            "ALT+F4": (ALT, 0x3D, "ALT+F4"),
            "CTRL+MINUS": (CTRL, 0x2D, "CTRL+MINUS"),
            "ESCAPE": (0, 0x29, "ESCAPE"),
            "PAGE_UP": (0, 0x4B, "PAGE_UP"),
        }
        for text, expected in cases.items():
            if expected is None:
                continue
            with self.subTest(text=text):
                self.assertEqual(config.parse_combo(text, "t"), expected)

    def test_digits_as_ints(self):
        self.assertEqual(config.parse_combo(1, "t"), (0, 0x1E, "1"))
        self.assertEqual(config.parse_combo(0, "t"), (0, 0x27, "0"))
        self.assertEqual(config.parse_combo("CMD+9", "t"), (GUI, 0x26, "CMD+9"))

    def test_key_table(self):
        for name, code in config.KEYS.items():
            self.assertEqual(config.parse_combo(name.lower(), "t")[1], code)
        punctuation = {"MINUS": 0x2D, "EQUAL": 0x2E, "LBRACKET": 0x2F, "RBRACKET": 0x30,
                       "BACKSLASH": 0x31, "SEMICOLON": 0x33, "QUOTE": 0x34, "GRAVE": 0x35,
                       "COMMA": 0x36, "PERIOD": 0x37, "SLASH": 0x38}
        for name, code in punctuation.items():
            self.assertEqual(config.KEYS[name], code)
        for n in range(13, 25):
            self.assertEqual(config.KEYS[f"F{n}"], 0x68 + n - 13)
        self.assertEqual(config.KEYS["INSERT"], 0x49)
        self.assertEqual(config.KEYS["PRINT_SCREEN"], 0x46)
        self.assertEqual(config.KEYS["CAPS_LOCK"], 0x39)

    def test_combo_rejections(self):
        for value in ("RIGTH", "CTRL+", "+C", "CTRL++C", "", "A+B", "C+CTRL", "F24+SHIFT",
                      "CTRL+CTRL+C", "CTRL+CONTROL+C", "SHIFT+SHIFT", "CMD+WIN+C", "F25",
                      True, False, 10, -1, 1.5, [], ["CTRL+C"], {}):
            with self.subTest(value=value), self.assertRaises(ValueError):
                config.parse_combo(value, "t")

    # --- text -------------------------------------------------------------
    def test_text_encoding(self):
        _, strokes = self.render_action({"text": "aZ1!?\"{ \n\t"})
        self.assertEqual(strokes, [
            (0, 0x04, 0), (SHIFT, 0x1D, 0), (0, 0x1E, 0), (SHIFT, 0x1E, 0),
            (SHIFT, 0x38, 0), (SHIFT, 0x34, 0), (SHIFT, 0x2F, 0), (0, 0x2C, 0),
            (0, 0x28, 0), (0, 0x2B, 0),
        ])

    def test_every_ascii_character_encodes(self):
        text = "".join(chr(c) for c in range(0x20, 0x7F))
        _, strokes = self.render_action({"text": text})
        self.assertEqual(len(strokes), 95)
        self.assertEqual(len(set(strokes)), 95)  # each character is a distinct stroke
        self.assertTrue(all(0x04 <= u <= 0x38 for _, u, _ in strokes))

    def test_text_rejections_name_character_and_location(self):
        with self.assertRaises(ValueError) as caught:
            self.render_action({"text": "abé"})
        message = str(caught.exception)
        self.assertIn("'é'", message)
        self.assertIn("index 2", message)
        self.assertIn("profiles[0].buttons.main.single.text", message)
        with self.assertRaises(ValueError) as caught:
            self.render_action({"sequence": [{"keys": "A"}, {"text": "x\r"}]})
        self.assertIn("'\\r'", str(caught.exception))
        self.assertIn("sequence[1].text", str(caught.exception))
        for bad in ("→", "\U0001F600", "\x00", "\x7f"):
            with self.subTest(bad=bad):
                self.assertRejected({"text": "a" + bad})
        for bad in ("", 5, True, ["a"], None):
            with self.subTest(text=bad):
                self.assertRejected({"text": bad})

    # --- actions & sequences ----------------------------------------------
    def test_action_shapes(self):
        self.assertIsNone(self.render_action(None))
        self.assertEqual(self.render_action("CMD+C")[1], [(GUI, 0x06, 0)])
        self.assertEqual(self.render_action({"keys": "F13", "label": "x"}), ("X", [(0, 0x68, 0)]))
        self.assertEqual(self.render_action(7)[1], [(0, 0x24, 0)])

    def test_action_rejections(self):
        for action in ({}, {"label": "X"}, {"keys": "A", "text": "a"}, {"keys": "A", "sequence": ["B"]},
                       {"keys": "A", "extra": 1}, True, 12, [], ["A"], 1.5, "",
                       {"keys": None}, {"keys": True}, {"keys": ["A"]}):
            with self.subTest(action=action):
                self.assertRejected(action)

    def test_sequence(self):
        label, strokes = self.render_action({"sequence": [
            {"text": "hi"}, "ENTER", {"keys": "CTRL+S"}, {"delay": 150}, {"text": "\n"}]})
        self.assertEqual(label, "MACRO")
        self.assertEqual(strokes, [
            (0, 0x0B, 0), (0, 0x0C, 0), (0, 0x28, 0), (CTRL, 0x16, 0), (0, 0, 150), (0, 0x28, 0)])

    def test_sequence_rejections(self):
        for sequence in ([], None, "ENTER", {"keys": "A"},
                         [{}], [{"keys": "A", "text": "b"}], [{"keys": "A", "label": "X"}],
                         [{"delay": 0}], [{"delay": 10001}], [{"delay": "5"}], [{"delay": True}],
                         [{"delay": 5.5}], [None], [True], [[]], [{"text": ""}], ["NOPE"]):
            with self.subTest(sequence=sequence):
                self.assertRejected({"sequence": sequence})
        self.assertEqual(self.render_action({"sequence": [{"delay": 10000}]})[1], [(0, 0, 10000)])
        self.assertEqual(self.render_action({"sequence": [{"delay": 1}]})[1], [(0, 0, 1)])

    def test_error_path_for_nested_step(self):
        self.data["profiles"] = [profile("A"), profile(
            "B", main_double={"sequence": ["A", "B", {"keys": "NOPE"}]})]
        with self.assertRaises(ValueError) as caught:
            config.render(self.data)
        self.assertIn("profiles[1].buttons.main.double.sequence[2].keys", str(caught.exception))

    def test_stroke_limits(self):
        self.assertEqual(len(self.render_action({"text": "a" * 4096})[1]), 4096)
        self.assertRejected({"text": "a" * 4097}, "4096")
        self.assertRejected({"sequence": [{"text": "a" * 4096}, "ENTER"]}, "4096")
        # 9 profiles x 6 slots x 700 = 37800 > 32768, but each macro is under its limit.
        slot = {"text": "a" * 700}
        self.data["profiles"] = [
            profile(f"P{i}", **{f"{b}_{g}": slot for b in ("main", "secondary")
                                for g in ("single", "double", "long")})
            for i in range(9)]
        with self.assertRaises(ValueError) as caught:
            config.render(self.data)
        self.assertIn("32768", str(caught.exception))
        for p in self.data["profiles"]:
            for b in p["buttons"].values():
                b["long"] = None
        config.render(self.data)  # 9 x 4 x 700 = 25200 fits

    # --- labels -----------------------------------------------------------
    def test_default_labels(self):
        self.assertEqual(self.render_action("cmd+shift+z")[0], "CMD+SHIFT~")  # 11 chars -> 9 + "~"
        self.assertEqual(self.render_action("cmd+shift+f")[0], "CMD+SHIFT+F"[:9] + "~")
        self.assertEqual(self.render_action("cmd+alt+z")[0], "CMD+ALT+Z")
        self.assertEqual(self.render_action("CTRL+ALT+C")[0], "CTRL+ALT+C")
        self.assertEqual(self.render_action({"text": "Hello\nsecond"})[0], "HELLO")
        self.assertEqual(self.render_action({"text": "0123456789"})[0], "0123456789")
        self.assertEqual(self.render_action({"text": "01234567890"})[0], "012345678~")
        self.assertEqual(self.render_action({"text": "\nabc"})[0], "MACRO")
        self.assertEqual(self.render_action({"sequence": ["A"]})[0], "MACRO")

    def test_explicit_labels(self):
        self.assertEqual(self.render_action({"keys": "A", "label": "paste"})[0], "PASTE")
        self.assertEqual(self.render_action({"text": "a", "label": "x y"})[0], "X Y")
        self.assertEqual(self.render_action({"sequence": ["A"], "label": "0123456789"})[0], "0123456789")

    def test_invalid_labels(self):
        for label in ("", "   ", "01234567890", "café", "a\nb", "\t", 5, True, None, ["A"]):
            with self.subTest(label=label):
                self.assertRejected({"keys": "A", "label": label})

    # --- profiles ---------------------------------------------------------
    def test_profile_count(self):
        self.data["profiles"] = []
        with self.assertRaises(ValueError):
            config.render(self.data)
        self.data["profiles"] = [profile(f"P{i}") for i in range(9)]
        self.assertIn("PROFILE_COUNT = 9;", config.render(self.data))
        self.data["profiles"].append(profile("P9"))
        with self.assertRaises(ValueError):
            config.render(self.data)
        for bad in (None, {}, "x"):
            self.data["profiles"] = bad
            with self.assertRaises(ValueError):
                config.render(self.data)

    def test_profile_names(self):
        for name in ("", "   ", "A" * 13, "café", "a\tb", "a\n", 5, True, None):
            self.data["profiles"] = [profile(name)]
            with self.subTest(name=name), self.assertRaises(ValueError):
                config.render(self.data)
        self.data["profiles"] = [profile("A" * 12), profile("  Pad ")]
        result = config.render(self.data)
        self.assertIn('{"Pad", {', result)
        for pair in (("Deck", "deck"), ("A", "A "), ("x", "X")):
            self.data["profiles"] = [profile(pair[0]), profile(pair[1])]
            with self.subTest(pair=pair), self.assertRaises(ValueError):
                config.render(self.data)

    def test_profile_fields(self):
        base = profile("A")
        broken = []
        no_name = copy.deepcopy(base); del no_name["name"]; broken.append(no_name)
        no_buttons = copy.deepcopy(base); del no_buttons["buttons"]; broken.append(no_buttons)
        extra = copy.deepcopy(base); extra["note"] = 1; broken.append(extra)
        no_secondary = copy.deepcopy(base); del no_secondary["buttons"]["secondary"]; broken.append(no_secondary)
        extra_button = copy.deepcopy(base); extra_button["buttons"]["third"] = {}; broken.append(extra_button)
        no_long = copy.deepcopy(base); del no_long["buttons"]["main"]["long"]; broken.append(no_long)
        triple = copy.deepcopy(base); triple["buttons"]["main"]["triple"] = "A"; broken.append(triple)
        for entry in broken + ["A", None, []]:
            self.data["profiles"] = [entry]
            with self.subTest(entry=entry), self.assertRaises(ValueError):
                config.render(self.data)

    def test_top_level_fields(self):
        for section in ("bluetooth", "display", "gestures", "typing", "profiles"):
            data = copy.deepcopy(self.data)
            del data[section]
            with self.subTest(section=section), self.assertRaises(ValueError):
                config.render(data)
        for stale in ("buttons", "extra"):
            data = copy.deepcopy(self.data)
            data[stale] = {}
            with self.subTest(stale=stale), self.assertRaises(ValueError):
                config.render(data)
        self.data["typing"]["extra"] = 1
        with self.assertRaises(ValueError):
            config.render(self.data)

    # --- retained scalar validation ---------------------------------------
    def test_typing_speed(self):
        for value in (4, 101, 0, "15", True, 15.0, None):
            self.setUp()
            self.data["typing"]["keystroke_ms"] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                config.render(self.data)
        for value in (5, 100):
            self.data["typing"]["keystroke_ms"] = value
            self.assertIn(f"KEYSTROKE_MS = {value};", config.render(self.data))

    def test_invalid_timing(self):
        for field, value in (("debounce_ms", 0), ("debounce_ms", True),
                             ("double_click_ms", 1500), ("long_press_ms", 250),
                             ("long_press_ms", "650")):
            data = copy.deepcopy(self.data)
            data["gestures"][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                config.render(data)

    def test_unicode_and_quotes_in_device_name(self):
        self.data["bluetooth"]["name"] = 'Slide "→" 🎤'
        result = config.render(self.data)
        self.assertIn('Slide \\"→\\" 🎤', result)
        self.assertNotIn("\\ud83c", result)

    def test_invalid_name_brightness(self):
        for name in ("", "a" * 30, "a\n", "😀" * 8, 123):
            self.data["bluetooth"]["name"] = name
            with self.subTest(name=name), self.assertRaises(ValueError):
                config.render(self.data)
        self.setUp()
        for brightness in (0, 256, True):
            self.data["display"]["brightness"] = brightness
            with self.assertRaises(ValueError):
                config.render(self.data)

    def test_string_literals_are_escaped(self):
        self.data["profiles"] = [profile('Q"\\?', main_single={"keys": "A", "label": '"??/\\'})]
        result = config.render(self.data)
        self.assertIn('{"Q\\"\\\\\\?", {', result)
        self.assertIn('{"\\"\\?\\?/\\\\", P0_MAIN_SINGLE, 1}', result)
        self.assertNotIn("??", result)

    # --- files ------------------------------------------------------------
    def test_reject_malformed_and_duplicate_yaml(self):
        with tempfile.TemporaryDirectory() as temp:
            source, destination = Path(temp) / "config.yaml", Path(temp) / "generated.h"
            for text in ("profiles: [", "bluetooth: null\nbluetooth: null", "null", "? [a,b]\n: c"):
                source.write_text(text)
                with self.subTest(text=text), self.assertRaises(ValueError):
                    config.generate(source, destination)
                self.assertFalse(destination.exists())

    def test_deterministic_output_and_no_write_when_unchanged(self):
        self.assertEqual(config.render(self.data), config.render(copy.deepcopy(self.data)))
        with tempfile.TemporaryDirectory() as temp:
            dest = Path(temp) / "include" / "generated.h"
            config.generate(ROOT / "config.yaml", dest)
            before = dest.stat().st_mtime_ns
            config.generate(ROOT / "config.yaml", dest)
            self.assertEqual(dest.stat().st_mtime_ns, before)
            self.assertEqual(dest.read_text(encoding="utf-8"), config.render(self.data))


if __name__ == "__main__":
    unittest.main()
