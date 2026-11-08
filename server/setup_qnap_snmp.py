#!/usr/bin/env python3
"""Interactive, non-echoing SNMPv3 credential setup for local InfoTAB collector."""
from __future__ import annotations
from getpass import getpass
import json
import os
from pathlib import Path
import re
import tempfile

destination = Path.home() / ".config" / "infotab" / "nas-snmp.json"
username = input("SNMPv3 QNAP username [infotab]: ").strip() or "infotab"
if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", username):
    raise SystemExit("Invalid SNMPv3 username. Use 1-32 letters, digits, '-' or '_'.")
auth = getpass("SNMPv3 authentication password (HMAC-SHA): ")
priv = getpass("SNMPv3 encryption password (AES): ")
privacy_protocol = input("Privacy protocol [DES] (AES if NAS supports it): ").strip().lower() or "des"
if privacy_protocol not in ("des", "aes"):
    raise SystemExit("Privacy protocol must be DES or AES.")
if not all(8 <= len(s) <= 64 and s.isascii() for s in (auth, priv)):
    raise SystemExit("Passwords must be 8-64 ASCII characters.")
if auth == priv:
    raise SystemExit("Use different authentication and encryption passwords.")
destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
os.chmod(destination.parent, 0o700)
with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=destination.parent, delete=False) as handle:
    name = handle.name
    os.fchmod(handle.fileno(), 0o600)
    json.dump({"username": username, "auth_password": auth, "priv_password": priv,
               "privacy_protocol": privacy_protocol}, handle)
    handle.flush()
    os.fsync(handle.fileno())
os.replace(name, destination)
os.chmod(destination, 0o600)
print("SNMPV3_CONFIG_STORED_SUCCESSFULLY; no credentials printed.")
