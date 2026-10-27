"""Trigger the firmware's boot-time reset hatch (wipe WiFi + config).

    python scripts/wifi_reset.py [PORT]      # PORT defaults to the only USB serial port

The main firmware listens for an 'r' during the first 2 s of boot; on seeing
it, it erases the stored WiFi credentials and app config and reboots, so the
next boot opens a clean config portal. Use this when the saved network is
gone (or the password changed) and the portal never opens, e.g. the serial
log keeps showing 4WAY_HANDSHAKE_TIMEOUT.
"""

import sys
import time

import serial
from _port import resolve_port


def main():
    port = resolve_port(sys.argv[1] if len(sys.argv) > 1 else None)

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

    # The app starts ~1 s after the reset; its escape window is ~1.0-3.0 s.
    # Press 'r' only inside that window, then stop, so the reboot that follows
    # a wipe is not immediately wiped again.
    time.sleep(1.0)
    t0 = time.time()
    while time.time() - t0 < 0.8:
        s.write(b"r")
        s.flush()
        time.sleep(0.04)

    end = time.time() + 10
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
