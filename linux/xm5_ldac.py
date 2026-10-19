#!/usr/bin/env python3
"""Select and verify an XM5 LDAC profile provided by BlueZ and PipeWire.

This uses the distribution's Bluetooth and audio stack. It does not install a
kernel driver, replace the radio driver, or implement an A2DP transport.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time


MAC = re.compile(r"^[0-9A-F]{2}(?::[0-9A-F]{2}){5}$")


class LdacError(Exception):
    pass


def normalize_address(value):
    address = value.replace("_", ":").replace("-", ":").upper()
    if not MAC.fullmatch(address):
        raise LdacError("Expected a Bluetooth address such as AA:BB:CC:DD:EE:FF.")
    return address


def command(*args):
    env = os.environ.copy()
    env["LC_ALL"] = "C"
    try:
        completed = subprocess.run(
            args, capture_output=True, text=True, encoding="utf-8", errors="replace",
            timeout=15, env=env, check=False,
        )
    except FileNotFoundError as exc:
        raise LdacError(f"Missing command: {args[0]}. Install PipeWire, WirePlumber, BlueZ and pactl.") from exc
    except subprocess.TimeoutExpired as exc:
        raise LdacError(f"Timed out running {args[0]}.") from exc
    if completed.returncode:
        message = (completed.stderr or completed.stdout).strip()
        raise LdacError(f"{args[0]} {' '.join(args[1:])} failed: {message or completed.returncode}")
    return completed.stdout


def pactl_json(kind):
    try:
        data = json.loads(command("pactl", "--format=json", "list", kind))
    except json.JSONDecodeError as exc:
        raise LdacError("pactl did not return JSON; install a current pulseaudio-utils/pactl package.") from exc
    if not isinstance(data, list):
        raise LdacError(f"Unexpected pactl {kind} response.")
    return data


def pipewire_server():
    info = command("pactl", "info")
    match = re.search(r"^Server Name:\s*(.*)$", info, re.MULTILINE)
    name = match.group(1).strip() if match else "unknown"
    if "pipewire" not in name.lower():
        raise LdacError(f"Audio server is {name}; this tool requires PipeWire's PulseAudio service.")
    return name


def card_address(card):
    name = str(card.get("name", ""))
    if not name.startswith("bluez_card."):
        return None
    properties = card.get("properties") or {}
    candidates = (
        properties.get("api.bluez5.address"),
        properties.get("device.string"),
        name.removeprefix("bluez_card.").split(".", 1)[0],
    )
    for candidate in candidates:
        if isinstance(candidate, str):
            try:
                return normalize_address(candidate)
            except LdacError:
                pass
    return None


def bluetooth_cards(cards):
    return [card for card in cards if card_address(card)]


def find_card(cards, address):
    matches = [card for card in bluetooth_cards(cards) if card_address(card) == address]
    if len(matches) != 1:
        if not matches:
            raise LdacError(f"No connected Bluetooth audio card for {address}. Pair and connect the XM5 first.")
        raise LdacError(f"Multiple Bluetooth audio cards match {address}; refusing to choose one.")
    return matches[0]


def profile_items(card):
    profiles = card.get("profiles") or {}
    if isinstance(profiles, dict):
        return list(profiles.items())
    if isinstance(profiles, list):
        return [(item.get("name", ""), item) for item in profiles if isinstance(item, dict)]
    raise LdacError("Unexpected pactl card profile format.")


def active_profile(card):
    active = card.get("active_profile") or ""
    return str(active.get("name", "")) if isinstance(active, dict) else str(active)


def ldac_profile(card):
    candidates = []
    for name, details in profile_items(card):
        if "a2dp" not in name.lower() or "ldac" not in name.lower():
            continue
        availability = str((details or {}).get("available", "unknown")).lower()
        if availability not in ("no", "false", "0"):
            candidates.append(name)
    for preferred in ("a2dp-sink-ldac", "a2dp_sink_ldac"):
        if preferred in candidates:
            return preferred
    return sorted(candidates)[0] if candidates else None


def card_sinks(card, sinks):
    index = str(card.get("index", ""))
    suffix = card_address(card).replace(":", "_")
    matches = []
    for sink in sinks:
        name = str(sink.get("name", ""))
        belongs = str(sink.get("card", "")) == index and index != ""
        if not belongs:
            belongs = name.startswith((f"bluez_output.{suffix}.", f"bluez_sink.{suffix}."))
        if belongs:
            matches.append(sink)
    return matches


def pipewire_codec(sink):
    properties = sink.get("properties") or {}
    for key in ("api.bluez5.codec", "bluez5.codec"):
        if key in properties:
            return str(properties[key]).lower()
    if not shutil.which("pw-dump"):
        return None
    try:
        objects = json.loads(command("pw-dump"))
    except (LdacError, json.JSONDecodeError):
        return None
    for obj in objects if isinstance(objects, list) else []:
        props = (obj.get("info") or {}).get("props") or {}
        if props.get("node.name") == sink.get("name"):
            codec = props.get("api.bluez5.codec") or props.get("bluez5.codec")
            if codec:
                return str(codec).lower()
    return None


def verify(card, sinks):
    profile = active_profile(card)
    if "a2dp" not in profile.lower() or "ldac" not in profile.lower():
        raise LdacError(f"Active profile is {profile or 'none'}, not LDAC.")
    matches = card_sinks(card, sinks)
    if len(matches) != 1:
        raise LdacError(f"Expected one XM5 playback sink; found {len(matches)}.")
    codec = pipewire_codec(matches[0])
    if codec is not None and codec != "ldac":
        raise LdacError(f"PipeWire reports codec {codec}, not LDAC.")
    return matches[0], codec


def show_card(card):
    props = card.get("properties") or {}
    address = card_address(card)
    label = props.get("device.description") or props.get("bluez.alias") or card["name"]
    profile = active_profile(card) or "none"
    available = ldac_profile(card)
    print(f"{address}  {label}\n  card: {card['name']}\n  active: {profile}\n  LDAC profile: {available or 'unavailable'}")


def activate(address, make_default=False, wait_seconds=8):
    pipewire_server()
    card = find_card(pactl_json("cards"), address)
    profile = ldac_profile(card)
    if not profile:
        names = ", ".join(name for name, _ in profile_items(card))
        raise LdacError(
            f"No available LDAC profile for {address}. Profiles: {names or 'none'}. "
            "Check that the XM5 is connected for media audio and PipeWire has LDAC support."
        )
    previous = active_profile(card)
    changed = previous != profile
    if changed:
        command("pactl", "set-card-profile", str(card["name"]), profile)
    deadline = time.monotonic() + wait_seconds
    failure = None
    while True:
        try:
            current = find_card(pactl_json("cards"), address)
            sink, codec = verify(current, pactl_json("sinks"))
            break
        except LdacError as exc:
            failure = exc
            if time.monotonic() >= deadline:
                if changed and previous:
                    try:
                        command("pactl", "set-card-profile", str(card["name"]), previous)
                    except LdacError as rollback_error:
                        raise LdacError(f"{failure}; restoring {previous} failed: {rollback_error}") from exc
                    raise LdacError(f"{failure}; restored previous profile {previous}.") from exc
                raise
            time.sleep(0.25)
    print(f"LDAC profile active for {address}: {active_profile(current)}")
    print(f"Playback sink: {sink['name']}")
    print(f"PipeWire codec property: {codec or 'not exposed (profile checked)'}")
    if make_default:
        old_default = command("pactl", "get-default-sink").strip()
        command("pactl", "set-default-sink", str(sink["name"]))
        new_default = command("pactl", "get-default-sink").strip()
        if new_default != sink["name"]:
            raise LdacError(f"Default sink did not change to {sink['name']}.")
        print(f"Default sink: {new_default}")
        if old_default != new_default:
            print(f"To restore the previous default: pactl set-default-sink {old_default}")
    print("This checks PipeWire's selected codec/profile; confirm audible playback on the laptop.")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("doctor", help="Check PipeWire and list connected Bluetooth audio cards")
    sub.add_parser("list", help="List connected Bluetooth audio cards and LDAC profiles")
    for action in ("status", "activate"):
        p = sub.add_parser(action)
        p.add_argument("--address", required=True, help="Exact XM5 Bluetooth address")
        if action == "activate":
            p.add_argument("--default", action="store_true", help="Set the XM5 as the default audio output")
    args = parser.parse_args(argv)
    if sys.platform != "linux":
        raise LdacError("Run this tool in a Linux desktop session on the test laptop.")
    if args.action == "doctor":
        print(f"Audio server: {pipewire_server()}")
    if args.action in ("doctor", "list"):
        cards = bluetooth_cards(pactl_json("cards"))
        if not cards:
            print("No connected Bluetooth audio cards. Pair and connect the XM5, then try again.")
        for card in cards:
            show_card(card)
        return
    address = normalize_address(args.address)
    if args.action == "activate":
        activate(address, args.default)
        return
    pipewire_server()
    card = find_card(pactl_json("cards"), address)
    show_card(card)
    sink, codec = verify(card, pactl_json("sinks"))
    print(f"Playback sink: {sink['name']}")
    print(f"PipeWire codec property: {codec or 'not exposed (profile checked)'}")


if __name__ == "__main__":
    try:
        main()
    except LdacError as exc:
        print(f"LDAC setup: {exc}", file=sys.stderr)
        sys.exit(2)
