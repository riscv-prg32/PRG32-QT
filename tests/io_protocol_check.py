#!/usr/bin/env python3
"""Check a running PRG32-QT device server against the firmware's prg32-io-1.0 rules.

Start the desktop app first (it listens on 127.0.0.1:8081), then:

    python3 tests/io_protocol_check.py tests/fixtures/Asteroids.prg32

Every expectation below is the reply of components/prg32/prg32_usb_protocol.c in PRG32 SDK
1fa8474. The cartridge is stored in slot cart7 and started once; use a cartridge without sound
when nobody should hear it.
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time
import zlib

MAGIC = b"PRG32IO\0"
HEADER = struct.Struct("<8sBBHIII")
HELLO, BEGIN, DATA, END, RUN, KEY = 0x01, 0x02, 0x03, 0x04, 0x05, 0x0B
STATUS = 0x80


class Client:
    def __init__(self, host: str, port: int) -> None:
        self.sock = socket.create_connection((host, port), timeout=10)
        self.request = 0
        self.buffer = b""

    def call(self, kind: int, payload: bytes | dict = b"") -> dict:
        if isinstance(payload, dict):
            payload = json.dumps(payload).encode()
        self.request += 1
        self.sock.sendall(HEADER.pack(MAGIC, 1, kind, 0, self.request, len(payload), zlib.crc32(payload)) + payload)
        while True:
            while len(self.buffer) < HEADER.size:
                self.buffer += self.sock.recv(65536)
            _, _, reply_type, _, request, size, _ = HEADER.unpack_from(self.buffer)
            while len(self.buffer) < HEADER.size + size:
                self.buffer += self.sock.recv(65536)
            body = self.buffer[HEADER.size : HEADER.size + size]
            self.buffer = self.buffer[HEADER.size + size :]
            if request == self.request and reply_type == STATUS:
                return json.loads(body)

    def close(self) -> None:
        self.sock.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("cartridge")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8081)
    args = parser.parse_args()
    image = open(args.cartridge, "rb").read()
    failures: list[str] = []

    def expect(label: str, reply: dict, ok: bool, text: str | None = None) -> None:
        message = reply.get("error" if not ok else "message", "")
        good = reply.get("ok") is ok and (text is None or message == text)
        print(("PASS " if good else "FAIL ") + label + ": " + json.dumps(reply))
        if not good:
            failures.append(label)

    client = Client(args.host, args.port)
    hello = client.call(HELLO)
    expect("hello", hello, True)
    if hello.get("protocol") != "prg32-io-1.0":
        failures.append("hello protocol")
    begin = {"name": "io-check", "slot": "cart7", "size": len(image), "crc32": zlib.crc32(image), "command": "store"}
    expect("begin without crc32", client.call(BEGIN, {k: v for k, v in begin.items() if k != "crc32"}), False,
           "invalid upload metadata")
    expect("begin without name", client.call(BEGIN, {k: v for k, v in begin.items() if k != "name"}), False,
           "invalid upload metadata")
    expect("begin unknown command", client.call(BEGIN, dict(begin, command="erase")), False, "invalid upload metadata")
    expect("begin slot cart8", client.call(BEGIN, dict(begin, slot="cart8")), False, "invalid upload metadata")
    expect("begin slot cart07", client.call(BEGIN, dict(begin, slot="cart07")), False, "invalid upload metadata")
    expect("data without upload", client.call(DATA, struct.pack("<I", 0) + image[:16]), False, "no upload in progress")
    expect("end without upload", client.call(END), False, "no upload in progress")
    expect("run without slot", client.call(RUN, {}), False, "invalid cartridge slot")

    expect("begin", client.call(BEGIN, begin), True, "upload accepted")
    expect("run during upload", client.call(RUN, {"slot": "cart7"}), False, "upload in progress")
    expect("key during upload", client.call(KEY, {"keys": ["A"]}), False, "upload in progress")
    expect("empty chunk", client.call(DATA, struct.pack("<I", 0)), False, "invalid upload chunk")
    expect("oversized chunk", client.call(DATA, struct.pack("<I", 0) + bytes(4097)), False, "invalid upload chunk")
    offset = 0
    previous = b""
    while offset < len(image):
        chunk = image[offset : offset + 4096]
        reply = client.call(DATA, struct.pack("<I", offset) + chunk)
        if not reply.get("ok"):
            expect(f"chunk at {offset}", reply, True)
            break
        previous = struct.pack("<I", offset) + chunk
        offset += len(chunk)
    expect("resent last chunk", client.call(DATA, previous), True)
    expect("end", client.call(END), True)
    expect("run cart7", client.call(RUN, {"slot": "cart7"}), True, "cartridge selected")
    expect("key", client.call(KEY, {"keys": ["A"], "hold_ms": 50}), True)

    # A host that disconnects mid-upload must not leave the server refusing KEY and RUN.
    expect("begin then disconnect", client.call(BEGIN, begin), True, "upload accepted")
    client.close()
    time.sleep(0.5)
    client = Client(args.host, args.port)
    expect("key after abandoned upload", client.call(KEY, {"action": "release"}), True)
    client.close()

    print("FAILED: " + ", ".join(failures) if failures else "ALL PASSED")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
