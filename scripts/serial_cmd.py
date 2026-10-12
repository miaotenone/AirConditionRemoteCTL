"""Drive the main firmware's serial console and print what it says.

    python scripts/serial_cmd.py [PORT] ["cmd1" "cmd2" ...]

Commands understood by the firmware console (see src/main.cpp):

    scan                 list visible WiFi networks
    show                 print the current config (passwords masked)
    set <key> <value>    set one field and persist it   (keys: ssid pass host
                         port user mqttpass prefix name brand)
    reset                wipe WiFi + config and restart
    restart              reboot (applies a changed SSID / MQTT host)

Resets the board first so the boot banner is included; the 2 s boot-time 'r'
escape window is waited out before any command is sent. Use --no-reset to
talk to a board that is already running.
"""
import argparse
import sys
import time

import serial


def read_for(ser, secs, quiet=0.6):
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", nargs="?", default="COM16")
    ap.add_argument("--no-reset", action="store_true")
    ap.add_argument("--wait", type=float, default=3.0, help="seconds to read per command")
    ap.add_argument("cmds", nargs="*", help="console commands to send")
    args = ap.parse_args()

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = 115200
    ser.timeout = 0.1
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.dtr = False
    ser.rts = False

    if not args.no_reset:
        ser.rts = True
        time.sleep(0.12)
        ser.rts = False
        sys.stdout.write(read_for(ser, 5.0))   # banner + boot 'r' window

    for c in args.cmds:
        ser.write((c + "\n").encode())
        ser.flush()
        print(f"\n--- $ {c}")
        sys.stdout.write(read_for(ser, args.wait))

    ser.close()


if __name__ == "__main__":
    main()
