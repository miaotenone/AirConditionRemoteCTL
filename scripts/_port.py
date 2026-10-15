"""Work out which serial port the AC gateway is on.

The board's COM number is **not stable**: unplugging and replugging it (or
using a different USB socket) makes Windows hand out a fresh one -- COM16
became COM11 exactly that way. So nothing here hardcodes a port.

Resolution order:

  1. an explicit argument, or the ``AC_PORT`` environment variable
  2. the only USB serial port present
  3. otherwise stop, list the candidates, and ask for one

Bluetooth and on-board UARTs are ignored. The ESP32 always enumerates as a
USB serial device (CH340 / CP210x / FTDI), i.e. an ``hwid`` containing
``USB`` -- never as a ``BTHENUM`` device, which is what makes the filter
safe on a laptop with several phantom Bluetooth COM ports.
"""
import os
import sys

ENV_VAR = "AC_PORT"


def _candidates():
    """USB serial ports currently present, in a stable order."""
    from serial.tools import list_ports

    found = [p for p in list_ports.comports() if "USB" in (p.hwid or "").upper()]
    return sorted(found, key=lambda p: p.device)


def _describe(ports):
    return "\n".join(f"  {p.device}  {p.description}" for p in ports)


def _usage_hint(port):
    prog = os.path.basename(sys.argv[0] or "script.py")
    return (
        f"  python scripts/{prog} {port}\n"
        f"or set {ENV_VAR}={port}"
    )


def resolve_port(explicit=None):
    """Return the port to talk to, or exit with an actionable message."""
    port = explicit or os.environ.get(ENV_VAR)
    if port:
        return port

    ports = _candidates()
    if len(ports) == 1:
        return ports[0].device

    if not ports:
        sys.exit(
            "No USB serial port found -- is the gateway plugged in?\n"
            "Check the cable and the USB-serial driver (CH340/CP210x).\n"
            f"{_usage_hint('<PORT, e.g. COM11>')}"
        )

    sys.exit(
        f"{len(ports)} USB serial ports found, so I can't guess which is the\n"
        f"gateway -- pick the right one from this list:\n"
        f"{_describe(ports)}\n"
        f"{_usage_hint('<PORT>')}"
    )
