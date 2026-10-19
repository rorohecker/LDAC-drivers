"""Safety checks for selecting a Bluetooth card and changing its audio route."""

import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "linux" / "xm5_ldac.py"
spec = importlib.util.spec_from_file_location("xm5_ldac", SCRIPT)
ldac = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ldac)

ADDRESS = "AA:BB:CC:DD:EE:FF"
CARD = {
    "index": 42,
    "name": "bluez_card.AA_BB_CC_DD_EE_FF",
    "properties": {"device.description": "WH-1000XM5", "device.string": ADDRESS},
    "profiles": {
        "a2dp-sink-aac": {"available": "yes"},
        "a2dp-sink-ldac": {"available": "yes"},
    },
    "active_profile": "a2dp-sink-aac",
}
SINK = {
    "index": 43,
    "card": 42,
    "name": "bluez_output.AA_BB_CC_DD_EE_FF.1",
    "properties": {"api.bluez5.codec": "ldac"},
}


class LinuxLdacTests(unittest.TestCase):
    def test_exact_address_and_available_profile(self):
        self.assertEqual(ldac.card_address(CARD), ADDRESS)
        self.assertEqual(ldac.ldac_profile(CARD), "a2dp-sink-ldac")
        with self.assertRaises(ldac.LdacError):
            ldac.find_card([CARD], "AA:BB:CC:DD:EE:00")
        unavailable = dict(CARD, profiles={"a2dp-sink-ldac": {"available": "no"}})
        self.assertIsNone(ldac.ldac_profile(unavailable))

    def test_verification_rejects_wrong_codec_or_sink(self):
        current = dict(CARD, active_profile="a2dp-sink-ldac")
        self.assertEqual(ldac.verify(current, [SINK])[0]["name"], SINK["name"])
        with self.assertRaisesRegex(ldac.LdacError, "not LDAC"):
            ldac.verify(current, [dict(SINK, properties={"api.bluez5.codec": "aac"})])
        with self.assertRaisesRegex(ldac.LdacError, "found 0"):
            ldac.verify(current, [dict(SINK, card=99, name="other_output")])

    def test_activate_switches_exact_card_after_verification(self):
        calls = []
        active = dict(CARD)

        def fake_command(*args):
            calls.append(args)
            if args[:2] == ("pactl", "set-card-profile"):
                active["active_profile"] = args[3]
            if args[:2] == ("pactl", "get-default-sink"):
                return "old_sink\n" if len([x for x in calls if x[:2] == ("pactl", "get-default-sink")]) == 1 else SINK["name"] + "\n"
            return ""

        def fake_json(kind):
            return [active] if kind == "cards" else [SINK]

        with patch.object(ldac, "pipewire_server", return_value="PipeWire"), \
             patch.object(ldac, "command", side_effect=fake_command), \
             patch.object(ldac, "pactl_json", side_effect=fake_json):
            ldac.activate(ADDRESS, make_default=True, wait_seconds=0)
        self.assertIn(("pactl", "set-card-profile", CARD["name"], "a2dp-sink-ldac"), calls)
        self.assertIn(("pactl", "set-default-sink", SINK["name"]), calls)

    def test_failed_activation_restores_previous_profile(self):
        calls = []
        active = dict(CARD)

        def fake_command(*args):
            calls.append(args)
            if args[:2] == ("pactl", "set-card-profile"):
                active["active_profile"] = args[3]
            return ""

        def fake_json(kind):
            if kind == "cards":
                return [active]
            return [dict(SINK, properties={"api.bluez5.codec": "aac"})]

        with patch.object(ldac, "pipewire_server", return_value="PipeWire"), \
             patch.object(ldac, "command", side_effect=fake_command), \
             patch.object(ldac, "pactl_json", side_effect=fake_json):
            with self.assertRaisesRegex(ldac.LdacError, "restored previous profile"):
                ldac.activate(ADDRESS, wait_seconds=0)
        self.assertEqual(active["active_profile"], "a2dp-sink-aac")
        self.assertFalse(any(x[:2] == ("pactl", "set-default-sink") for x in calls))


if __name__ == "__main__":
    unittest.main()
