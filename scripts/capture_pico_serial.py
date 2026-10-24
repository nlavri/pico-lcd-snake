#!/usr/bin/env python3
"""Capture a Pico USB CDC port to both stdout and a workspace log file."""

import argparse
import errno
import glob
import os
import select
import sys
import termios
import time
import tty
from typing import Optional


def find_port(pattern: str) -> Optional[str]:
    ports = sorted(glob.glob(pattern))
    return ports[0] if ports else None


def open_serial(port: str, baud: int) -> int:
    fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd)
    attributes = termios.tcgetattr(fd)
    speed = getattr(termios, f"B{baud}")
    attributes[4] = speed
    attributes[5] = speed
    termios.tcsetattr(fd, termios.TCSANOW, attributes)
    return fd


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbmodem*")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--log", required=True)
    args = parser.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.log)), exist_ok=True)
    with open(args.log, "wb", buffering=0) as log:
        fd = None
        active_port = None
        announced_wait = False

        try:
            while True:
                if fd is None:
                    port = find_port(args.port)
                    if port is None:
                        if not announced_wait:
                            print(f"[capture] Waiting for {args.port}", flush=True)
                            announced_wait = True
                        time.sleep(0.2)
                        continue

                    try:
                        fd = open_serial(port, args.baud)
                    except OSError:
                        time.sleep(0.2)
                        continue

                    active_port = port
                    announced_wait = False
                    print(f"[capture] Connected to {port}; logging to {args.log}", flush=True)

                try:
                    readable, _, _ = select.select([fd], [], [], 0.5)
                    if not readable:
                        continue
                    data = os.read(fd, 4096)
                    if not data:
                        raise OSError(errno.ENXIO, "device disconnected")
                    log.write(data)
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                except OSError as error:
                    if error.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                        continue
                    if error.errno not in (errno.EBADF, errno.EIO, errno.ENODEV, errno.ENXIO):
                        raise
                    os.close(fd)
                    fd = None
                    print(f"\n[capture] Disconnected from {active_port}; waiting for reconnect", flush=True)
                    active_port = None
        except KeyboardInterrupt:
            print("\n[capture] Stopped", flush=True)
        finally:
            if fd is not None:
                os.close(fd)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
