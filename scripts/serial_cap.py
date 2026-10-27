"""Capture the ESP32's serial output.

    python scripts/serial_cap.py [PORT] [SECONDS]

Pulses RTS to reset the board, then prints everything it says until the line
has been quiet for 0.6 s (or SECONDS elapse). Handy for grabbing the boot log
and the assigned IP without opening a monitor.
"""

import sys
import time

import serial
from _port import resolve_port


def main():
    port = resolve_port(sys.argv[1] if len(sys.argv) > 1 else None)
    secs = float(sys.argv[2]) if len(sys.argv) > 2 else 12.0

    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.1
    s.dtr = False
    s.rts = False
    s.open()
    s.dtr = False
    s.rts = False
    s.rts = True  # reset the board
    time.sleep(0.12)
    s.rts = False

    end = time.time() + secs
    buf = bytearray()
    while time.time() < end:
        n = s.in_waiting
        if n:
            buf += s.read(n)
            end = max(end, time.time() + 0.6)
        else:
            time.sleep(0.05)

    sys.stdout.write(buf.decode("utf-8", "replace"))
    s.close()


if __name__ == "__main__":
    main()
