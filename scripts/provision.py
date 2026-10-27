"""Provision the gateway's WiFi + MQTT settings over the USB serial console.

    python scripts/provision.py [PORT] --ssid NAME --pass PW --host IP [options]

    python scripts/provision.py --windows-profile HUAWEI-B311-98C3 \
                               --host 192.168.8.103

Options
    --ssid / --pass           WiFi network to join
    --windows-profile NAME    read SSID + password from a saved Windows WLAN
                              profile instead of typing them (uses netsh, and
                              never prints the password)
    --host / --mqtt-port      MQTT broker address (defaults to leaving it alone)
    --mqtt-user / --mqtt-pass MQTT credentials
    --prefix / --name / --brand   topic prefix, device name, IR brand code
    --no-reset                don't reset the board first (board must already be
                              inside its serial provisioning window)

The firmware exposes this console (see src/serial_console.cpp). The script
resets the board by default for a predictable starting point. The console is
also serviced during the non-blocking portal and normal operation; use
--no-reset to keep an already running board in place.

Passwords are masked in everything this script prints.
"""

import argparse
import subprocess
import sys
import time

import serial
from _port import resolve_port

KEY_MARKERS = ("关键内容", "Key Content")
SECRET_KEYS = ("pass", "mqttpass")


def read_for(ser, secs, quiet=0.5):
    """Read until the line has been quiet for `quiet` seconds, or `secs` elapse."""
    end = time.time() + secs
    buf = bytearray()
    while time.time() < end:
        n = ser.in_waiting
        if n:
            buf += ser.read(n)
            end = max(end, time.time() + quiet)
        else:
            time.sleep(0.05)
    return buf.decode("utf-8", "replace")


def read_windows_profile(name):
    """Return (ssid, password) for a saved Windows WLAN profile."""
    proc = subprocess.run(
        ["netsh", "wlan", "show", "profile", f"name={name}", "key=clear"],
        capture_output=True,
    )
    if proc.returncode != 0:
        raise SystemExit(f"netsh failed for profile {name!r}")

    # The console codepage varies (UTF-8 here, GBK on older Chinese Windows),
    # so try the likely encodings and take the first one that parses.
    text = None
    for encoding in ("utf-8", "gbk", "cp936", "utf-16-le"):
        try:
            candidate = proc.stdout.decode(encoding)
        except UnicodeDecodeError:
            continue
        if any(marker in candidate for marker in KEY_MARKERS):
            text = candidate
            break
    if text is None:
        raise SystemExit(f"could not decode the netsh output for profile {name!r}")

    for line in text.splitlines():
        if ":" not in line:
            continue
        label, _, value = line.partition(":")
        if any(marker in label for marker in KEY_MARKERS):
            value = value.strip()
            if value:
                return name, value
    raise SystemExit(f"could not read the password for profile {name!r}")


def masked(key, value):
    return "***" if key in SECRET_KEYS else value


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("port", nargs="?", help="defaults to the only USB serial port")
    ap.add_argument("--ssid")
    ap.add_argument("--pass", dest="pw")
    ap.add_argument("--windows-profile", metavar="NAME")
    ap.add_argument("--host")
    ap.add_argument("--mqtt-port", type=int)
    ap.add_argument("--mqtt-user")
    ap.add_argument("--mqtt-pass")
    ap.add_argument("--prefix")
    ap.add_argument("--name")
    ap.add_argument("--brand")
    ap.add_argument("--no-reset", action="store_true")
    args = ap.parse_args()

    ssid, pw = args.ssid, args.pw
    if args.windows_profile:
        ssid, pw = read_windows_profile(args.windows_profile)

    fields = []
    if ssid is not None:
        fields.append(("ssid", ssid))
    if pw is not None:
        fields.append(("pass", pw))
    if args.host is not None:
        fields.append(("host", args.host))
    if args.mqtt_port is not None:
        fields.append(("port", str(args.mqtt_port)))
    if args.mqtt_user is not None:
        fields.append(("user", args.mqtt_user))
    if args.mqtt_pass is not None:
        fields.append(("mqttpass", args.mqtt_pass))
    if args.prefix is not None:
        fields.append(("prefix", args.prefix))
    if args.name is not None:
        fields.append(("name", args.name))
    if args.brand is not None:
        fields.append(("brand", args.brand))

    if not fields:
        print("nothing to provision -- pass --ssid/--pass/--host/...", file=sys.stderr)
        return 2

    ser = serial.Serial()
    ser.port = resolve_port(args.port)
    ser.baudrate = 115200
    ser.timeout = 0.1
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.dtr = False
    ser.rts = False

    def send(cmd, print_as=None, wait=3.0):
        print(f"\n--- $ {print_as or cmd}")
        ser.write((cmd + "\n").encode())
        ser.flush()
        sys.stdout.write(read_for(ser, wait))

    try:
        if not args.no_reset:
            ser.rts = True
            time.sleep(0.12)
            ser.rts = False
            sys.stdout.write(read_for(ser, 5.0))  # banner + boot 'r' window

        send("show")
        for key, value in fields:
            send(f"set {key} {value}", print_as=f"set {key} {masked(key, value)}", wait=2.0)
        send("show")
        send("restart", wait=10.0)
    finally:
        ser.close()

    print("\nDone. If the board printed 'connected, IP=...' it is on the network now.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
