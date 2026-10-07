#!/usr/bin/env python3
"""Read the InfiniSense router telemetry stream and decode mesh events.

A router streams newline-delimited JSON on its USB CDC serial port at
115200 baud. Each line is either a liveness status object:

    {"status":"ok","node":123,"radio":1,"uptime_ms":45678}

or a received mesh event (payload is hex encoded):

    {"src":1234,"seq":56,"type":1,"len":1,"payload":"01"}

`type` is the mesh type: 1 = kBinaryEvent (payload byte 0, bit 0 is the input
state), 2 = kMeshBeacon (payload carried opaquely). See Firmware/docs/packet.md.

Usage:
    python3 read_telemetry.py                     # first /dev/ttyACM*
    python3 read_telemetry.py -p /dev/ttyACM0
    python3 read_telemetry.py -p COM5 -b 115200

Requires pyserial:  pip install pyserial
"""

import argparse
import glob
import json
import sys

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

MESH_TYPES = {1: "kBinaryEvent", 2: "kMeshBeacon"}


def find_port():
    """Return the first ttyACM*/ttyUSB* port, or None if there is none."""
    ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    return ports[0] if ports else None


def describe_event(msg):
    """Format one received-event JSON object as a human-readable line."""
    type_id = msg.get("type")
    type_name = MESH_TYPES.get(type_id, "unknown(%s)" % type_id)
    payload_hex = msg.get("payload", "")
    payload = bytes.fromhex(payload_hex) if payload_hex else b""

    line = "event src=%s seq=%s type=%s len=%s payload=%s" % (
        msg.get("src"),
        msg.get("seq"),
        type_name,
        msg.get("len"),
        payload_hex or "-",
    )
    if type_id == 1 and payload:
        line += " state=%d" % (payload[0] & 0x01)
    return line


def main():
    parser = argparse.ArgumentParser(
        description="Parse the InfiniSense router telemetry serial stream."
    )
    parser.add_argument(
        "-p", "--port", help="serial port (default: first /dev/ttyACM*)"
    )
    parser.add_argument("-b", "--baud", type=int, default=115200)
    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        sys.exit("no serial port found; pass --port /dev/ttyACM0")

    print("listening on %s @ %d" % (port, args.baud), file=sys.stderr)
    try:
        with serial.Serial(port, args.baud, timeout=1) as ser:
            # Loop forever: readline() returns b"" on the 1 s timeout, so keep
            # waiting rather than letting the io iterator stop on empty reads.
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").strip()
                if not text:
                    continue
                try:
                    msg = json.loads(text)
                except json.JSONDecodeError:
                    print("raw: %s" % text)
                    continue
                if msg.get("status"):
                    print(
                        "status node=%s radio=%s uptime_ms=%s"
                        % (
                            msg.get("node"),
                            msg.get("radio"),
                            msg.get("uptime_ms"),
                        )
                    )
                else:
                    print(describe_event(msg))
    except serial.SerialException as exc:
        sys.exit("serial error: %s" % exc)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
