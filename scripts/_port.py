"""Work out which serial port the AC gateway is on.

The board's COM number is **not stable**: unplugging and replugging it (or
using a different USB socket) makes Windows hand out a fresh one -- COM16
became COM11 exactly that way. So nothing here hardcodes a port.

Resolution order:

  1. an explicit argument, or the ``AC_PORT`` environment variable
  2. the only USB serial port present
  3. otherwise stop, list the candidates, and ask for one

A port named by (1) is also checked against what is actually plugged in, so a
stale ``AC_PORT`` from before a replug is called out immediately instead of
surfacing later as a cryptic "could not open port".

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


def _present():
    """Every serial port currently present (any kind), upper-cased."""
    from serial.tools import list_ports

    return {p.device.upper() for p in list_ports.comports()}


def _describe(ports):
    return "\n".join(f"  {p.device}  {p.description}" for p in ports)


def _usage_hint(port):
    prog = os.path.basename(sys.argv[0] or "script.py")
    return f"  python scripts/{prog} {port}\nor set {ENV_VAR}={port}"


def resolve_port(explicit=None):
    """Return the port to talk to, or exit with an actionable message."""
    if explicit:
        port, source = explicit, "the port argument"
    elif os.environ.get(ENV_VAR):
        port, source = os.environ[ENV_VAR], ENV_VAR
    else:
        port = source = None

    if port:
        present = _present()
        if port.upper() not in present:
            known = ", ".join(sorted(present)) or "none"
            sys.stderr.write(
                f"warning: {port} (from {source}) isn't plugged in; "
                f"present ports: {known}\n"
                f"         re-plugging the board changes its COM number -- "
                f"update {ENV_VAR} or drop it to auto-detect.\n"
            )
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
