# Linux LDAC trial with the built-in Bluetooth radio

The Linux path uses BlueZ, PipeWire, and WirePlumber's LDAC codec. It needs no
USB Bluetooth adapter, Windows driver, paid certificate, or boot security change.
The `linux/xm5_ldac.py` tool selects and checks the distribution's LDAC profile;
it does not encode or transmit Bluetooth audio itself. The burner laptop's radio,
XM5 connection, and audible playback still need a live test.

## Prerequisites

- A Linux desktop running PipeWire with its PulseAudio compatibility service,
  WirePlumber, and BlueZ.
- PipeWire's Bluetooth LDAC codec plugin and its libldac dependency. Some
  distributions put these in a separate Bluetooth audio package.
- Python 3.9 or newer and `pactl` with JSON output.
- WH-1000XM5 paired and connected for media playback through the laptop's
  built-in Bluetooth radio.

On a recent Ubuntu or Debian desktop, install any missing components from the
distribution repository:

```sh
sudo apt update
sudo apt install python3 bluez pipewire-audio wireplumber libspa-0.2-bluetooth pulseaudio-utils
```

Log out and back in if the audio service changed. The
[`libspa-0.2-bluetooth` package on Ubuntu](https://packages.ubuntu.com/noble/libspa-0.2-bluetooth)
includes the LDAC plugin, and its dependencies include the LDAC encoder;
[Debian packages the Bluetooth plugin separately](https://packages.debian.org/stable/libspa-0.2-bluetooth).
Check `pactl info` and confirm the server name contains `PipeWire` before
running the tool.

Use the desktop's Bluetooth settings to pair and connect the XM5. If needed,
`bluetoothctl devices` lists paired device addresses. Run the tool as your normal
desktop user, in the active graphical session, not with `sudo` or over an
inactive SSH session.

```sh
python3 linux/xm5_ldac.py doctor
python3 linux/xm5_ldac.py activate --address AA:BB:CC:DD:EE:FF --default
python3 linux/xm5_ldac.py status --address AA:BB:CC:DD:EE:FF
```

Replace the example address with the XM5 address shown by `doctor`. The tool
requires an exact address so another Bluetooth headset cannot be changed by
mistake. `activate` selects an available LDAC A2DP profile, waits for one
playback sink, and checks PipeWire's codec property when exposed. If profile
activation fails, it tries to restore the previous profile. `--default` sets
the XM5 as the output for new audio streams; omit it to leave the default output
alone. The tool prints the command for restoring the previous default output.

Play a short audio sample or music file and confirm sound reaches the XM5.
Run `status` during playback. An active LDAC profile and an `ldac` PipeWire
codec property are useful local evidence, but they are not an over-the-air
packet capture or an audible test. Note the active profile, codec property,
and whether playback was clear, stalled, or silent.

## If the LDAC profile is missing

1. Check that `doctor` reports PipeWire, and that the XM5 is connected. A
   paired but disconnected headset has no PipeWire card to switch.
2. Install your distribution's PipeWire Bluetooth audio codec package and
   libldac package if they are optional. WirePlumber lists `ldac` among its
   supported codecs, but only codecs built and available on the system can be
   selected.
3. If a microphone application opens the XM5 microphone, close it and retry.
   A2DP playback does not carry microphone audio; a headset profile may take
   over instead.
4. Reconnect the XM5, run `doctor` again, and use its exact address.

The command reports errors instead of silently falling back to SBC or AAC.
It changes only the current user's audio profile and, with `--default`, the
default output. It does not write WirePlumber configuration, modify firmware,
or claim the burner laptop has already passed a hardware test.

References: [WirePlumber Bluetooth codecs and LDAC quality](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/bluetooth.html),
[BlueZ pairing and connection commands](https://github.com/bluez/bluez/blob/master/doc/bluetoothctl.rst),
[pactl profile and output commands](https://manpages.debian.org/unstable/pulseaudio-utils/pactl.1.en.html).
