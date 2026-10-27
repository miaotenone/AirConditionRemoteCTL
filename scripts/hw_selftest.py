#!/usr/bin/env python3
"""Drive the ESP32 hardware self-test firmware over serial.

Resets the board, captures the boot banner, then sends a few self-test
commands and prints everything the firmware replies.

    python scripts/hw_selftest.py              # auto-detects the port
    python scripts/hw_selftest.py COM11 --no-reset --cmds h i o
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing:  python -m pip install pyserial")

from _port import resolve_port


def read_for(ser, seconds, quiet_gap=0.5):
    """Read until `seconds` pass, extending the deadline while data keeps coming."""
    end = time.time() + seconds
    buf = bytearray()
    while True:
        now = time.time()
        if now > end:
            break
        n = ser.in_waiting
        if n:
            buf += ser.read(n)
            end = max(end, time.time() + quiet_gap)
        else:
            time.sleep(0.02)
    return buf.decode("utf-8", errors="replace")


def reset_into_run(ser):
    """Pulse EN low->high with IO0 high: a normal (not bootloader) reset."""
    ser.dtr = False  # IO0 = high
    ser.rts = True  # EN  = low  -> hold in reset
    time.sleep(0.12)
    ser.rts = False  # EN  = high -> boot
    time.sleep(0.05)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", nargs="?", help="defaults to the only USB serial port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--no-reset", action="store_true")
    ap.add_argument("--cmds", nargs="*", default=["h", "i", "d", "r"])
    ap.add_argument("--wait", type=float, default=3.0)
    args = ap.parse_args()

    ser = serial.Serial()
    ser.port = resolve_port(args.port)
    ser.baudrate = args.baud
    ser.timeout = 0.1
    ser.dtr = False
    ser.rts = False
    ser.open()
    ser.dtr = False
    ser.rts = False

    if not args.no_reset:
        reset_into_run(ser)
        print("=============== boot banner ===============")
        print(read_for(ser, 3.0))

    for cmd in args.cmds:
        ser.write((cmd + "\r\n").encode())
        ser.flush()
        print(f"=============== cmd {cmd!r} ===============")
        print(read_for(ser, args.wait))
        print()

    ser.close()


if __name__ == "__main__":
    main()
