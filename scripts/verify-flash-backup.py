#!/usr/bin/env python3
"""Verify local full-flash InfoTAB backup without displaying stored secrets.

Only reads a file on disk; never connects to ESP32 or changes Flash.
Usage: python scripts/verify-flash-backup.py "path-to-flash-16mb.bin"
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys

FLASH_SIZE = 0x1000000  # 16 MiB
TABLE_OFFSET = 0x8000
TABLE_BYTES = 0x1000
ENTRY = struct.Struct("<HBBII16sI")
EXPECTED = {
    "nvs": (0x01, 0x02, 0x9000, 0x5000),
    "otadata": (0x01, 0x00, 0xE000, 0x2000),
    "app0": (0x00, 0x10, 0x10000, 0x140000),
    "app1": (0x00, 0x11, 0x150000, 0x140000),
}
def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    image = args.image

    if not image.is_file():
        print("BACKUP_FAIL: file not found")
        return 2
    size = image.stat().st_size
    if size != FLASH_SIZE:
        print(f"BACKUP_FAIL: expected {FLASH_SIZE} bytes; got {size}")
        return 3

    sha = hashlib.sha256()
    with image.open("rb") as f:
        while chunk := f.read(1024 * 1024):
            sha.update(chunk)
        f.seek(TABLE_OFFSET)
        table = f.read(TABLE_BYTES)

    partitions = {}
    for off in range(0, len(table), ENTRY.size):
        magic, typ, subtype, address, length, label_bytes, flags = ENTRY.unpack_from(table, off)
        if magic == 0xFFFF or magic == 0xEBEB:
            break
        if magic != 0x50AA:
            print(f"BACKUP_FAIL: unexpected partition-table magic at 0x{TABLE_OFFSET + off:X}")
            return 4
        name = label_bytes.split(b"\0", 1)[0].decode("ascii", errors="replace")
        if not name or name in partitions or address + length > FLASH_SIZE:
            print("BACKUP_FAIL: invalid or duplicate partition entry")
            return 5
        partitions[name] = (typ, subtype, address, length)

    for name, expected in EXPECTED.items():
        if partitions.get(name) != expected:
            print(f"BACKUP_FAIL: expected original layout for partition {name}")
            return 6

    print("[InfoTAB] Full flash backup VERIFIED")
    print(f"  Image length: {size} bytes (16 MiB)")
    print(f"  SHA256: {sha.hexdigest()}")
    print("  Original partitions: NVS/otadata/app0/app1 match running 4MB layout.")
    print("  No Wi-Fi SSID/password or NVS data have been printed.")
    print("  THIS IS A LOCAL BACKUP CHECK; NO FLASH WRITES.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
