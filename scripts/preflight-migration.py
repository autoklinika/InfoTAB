#!/usr/bin/env python3
"""Read-only InfoTAB migration risk audit of a PRIVATE local backup.

No serial connection, flash writes, secret extraction, NVS decoding, or file writes.
Input files stay on the Windows machine. Prints only partition metadata and
whether legacy partitions contain any non-erased (non-0xFF) bytes.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

TABLE_START = 0x8000
TABLE_LENGTH = 0x1000
FLASH_BYTES = 16 * 1024 * 1024
ENTRY = struct.Struct("<HBBII16sI")

def partitions(table: bytes, source: str) -> dict[str, tuple[int, int, int, int]]:
    result: dict[str, tuple[int, int, int, int]] = {}
    for pos in range(0, min(len(table), TABLE_LENGTH) - ENTRY.size + 1, ENTRY.size):
        magic, typ, sub, offset, size, rawlabel, flags = ENTRY.unpack_from(table, pos)
        if magic in (0xFFFF, 0xEBEB):
            break
        if magic != 0x50AA:
            raise ValueError(f"{source}: unexpected partition entry at 0x{pos:X}")
        name = rawlabel.split(b"\0", 1)[0].decode("ascii", "replace")
        if not name or name in result or offset + size > FLASH_BYTES or size == 0:
            raise ValueError(f"{source}: invalid/duplicate entry")
        result[name] = (typ, sub, offset, size)
    if not result:
        raise ValueError(f"{source}: missing partition entries")
    return result

def check() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--backup", required=True, type=Path)
    ap.add_argument("--candidate", required=True, type=Path)
    args = ap.parse_args()
    if not args.backup.is_file() or args.backup.stat().st_size != FLASH_BYTES:
        print("PREFLIGHT_FAIL: missing or incomplete private 16MiB backup")
        return 2
    if not args.candidate.is_file():
        print("PREFLIGHT_FAIL: missing 16MB candidate partitions.bin")
        return 3

    with args.backup.open("rb") as fp:
        fp.seek(TABLE_START)
        oldbytes = fp.read(TABLE_LENGTH)
    newbytes = args.candidate.read_bytes()
    try:
        old = partitions(oldbytes, "CURRENT_BACKUP")
        new = partitions(newbytes, "NEW_CANDIDATE")
    except ValueError as ex:
        print(f"PREFLIGHT_FAIL: {ex}")
        return 4

    expected_old = {
        "nvs": (1, 2, 0x9000, 0x5000),
        "otadata": (1, 0, 0xE000, 0x2000),
        "app0": (0, 0x10, 0x10000, 0x140000),
        "app1": (0, 0x11, 0x150000, 0x140000),
        "spiffs": (1, 0x82, 0x290000, 0x160000),
        "coredump": (1, 3, 0x3F0000, 0x10000),
    }
    expected_new = {
        "nvs": (1, 2, 0x9000, 0x5000),
        "otadata": (1, 0, 0xE000, 0x2000),
        "app0": (0, 0x10, 0x10000, 0x640000),
        "app1": (0, 0x11, 0x650000, 0x640000),
        "spiffs": (1, 0x82, 0xC90000, 0x360000),
        "coredump": (1, 3, 0xFF0000, 0x10000),
    }
    for label, values in expected_old.items():
        if old.get(label) != values:
            print(f"PREFLIGHT_FAIL: unexpected old partition {label}")
            return 5
    for label, values in expected_new.items():
        if new.get(label) != values:
            print(f"PREFLIGHT_FAIL: unexpected new partition {label}")
            return 6

    print("[InfoTAB] OFFLINE MIGRATION PREFLIGHT — read only")
    print("  Physical backup image: 16MiB, old partition layout confirmed")
    print("  Candidate: 16MiB A/B, app0 and app1 6400KiB each")
    print("  NVS: unchanged at offset 0x9000, size 20KiB")
    print("  OTA metadata: unchanged at 0xE000, size 8KiB")
    print("  Legacy storage overlap with new app0:")

    # Print *presence only*, never dump SPIFFS/NVS/app bytes.
    with args.backup.open("rb") as fp:
        for label in ("app1", "spiffs", "coredump"):
            _, _, offset, size = old[label]
            fp.seek(offset)
            region = fp.read(size)
            contains_data = any(byte != 0xFF for byte in region)
            status = "NON-ERASED DATA PRESENT" if contains_data else "appears erased (all 0xFF)"
            print(f"    {label:<8} 0x{offset:06X} size={size // 1024}KiB: {status}")

    print("  SECURITY STATE: unknown until on-device get-security-info is reviewed.")
    print("  BACKUP: read locally. No data was uploaded or decoded.")
    print("  RESULT: OFFLINE LAYOUT PASS; MIGRATION STILL BLOCKED.")
    print("  Never run PlatformIO Upload or erase_flash for this candidate yet.")
    return 0

if __name__ == "__main__":
    sys.exit(check())
