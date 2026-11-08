#!/usr/bin/env python3
"""Read-only QNAP NAS monitoring for InfoTAB.

No QTS credentials, SNMP passwords or writes appear in HTTP output.
SNMPv3 configuration is stored only in a local 0600 JSON file.
Unknown measurements are returned as '--' rather than fabricated.
"""
from __future__ import annotations

import asyncio
import json
import os
from pathlib import Path
import socket
import threading
import time

NAS_HOST = "192.168.1.101"
CONFIG_PATH = Path.home() / ".config/infotab/nas-snmp.json"
TTL_SECONDS = 20.0

# Read-only QNAP NAS-MIB (legacy 24681 systemInfoEx); validate against
# NAS.mib downloaded from the running NAS before claiming full coverage.
# Absence of an OID means 'unknown', not an unhealthy device.
OIDS = {
    "agent_uptime_ticks": "1.3.6.1.2.1.1.3.0",  # RFC 3418, SNMP agent only
    "cpu_pct": "1.3.6.1.4.1.24681.1.3.1.0",
    "mem_total": "1.3.6.1.4.1.24681.1.3.2.0",
    "mem_free": "1.3.6.1.4.1.24681.1.3.3.0",
    "uptime_ticks": "1.3.6.1.4.1.24681.1.3.4.0",
    "temp_cpu_c": "1.3.6.1.4.1.24681.1.3.5.0",
    "temp_system_c": "1.3.6.1.4.1.24681.1.3.6.0",
}
KEYS = (
    "nas_state", "snmp_state", "cpu_pct", "ram_pct", "temp_cpu_c",
    "temp_system_c", "disks", "raid", "free_gib", "fan_rpm",
    "uptime_days", "backup",
)


def empty_snapshot() -> dict[str, str]:
    return {
        "nas_state": "offline",
        "snmp_state": "not_configured",
        "cpu_pct": "--", "ram_pct": "--",
        "temp_cpu_c": "--", "temp_system_c": "--",
        "disks": "--", "raid": "--", "free_gib": "--",
        "fan_rpm": "--", "uptime_days": "--", "backup": "--",
    }


def reachable(host: str = NAS_HOST) -> bool:
    for port in (443, 8080):
        try:
            with socket.create_connection((host, port), timeout=0.6):
                return True
        except OSError:
            pass
    return False


def load_snmp_config(path: Path = CONFIG_PATH) -> dict[str, str] | None:
    try:
        stat = path.stat()
    except FileNotFoundError:
        return None
    if stat.st_uid != os.getuid() or stat.st_mode & 0o077:
        raise ValueError("SNMP config must be owned by current user and chmod 0600")
    with path.open(encoding="utf-8") as handle:
        config = json.load(handle)
    required = ("username", "auth_password", "priv_password")
    if not isinstance(config, dict) or any(
        not isinstance(config.get(key), str) or not config[key] for key in required
    ):
        raise ValueError("SNMPv3 configuration incomplete")
    result = {key: config[key] for key in required}
    protocol = config.get("privacy_protocol", "des").lower()
    if protocol not in ("des", "aes"):
        raise ValueError("Unsupported SNMP privacy protocol")
    result["privacy_protocol"] = protocol
    return result


def convert_metrics(values: dict[str, int]) -> dict[str, str]:
    out: dict[str, str] = {}
    cpu = values.get("cpu_pct")
    if cpu is not None and 0 <= cpu <= 100:
        out["cpu_pct"] = str(cpu)
    total, free = values.get("mem_total"), values.get("mem_free")
    if total and free is not None and 0 <= free <= total:
        out["ram_pct"] = str(round(100 * (total - free) / total))
    for key in ("temp_cpu_c", "temp_system_c"):
        val = values.get(key)
        if val is not None and -20 <= val <= 125:
            out[key] = str(val)
    ticks = values.get("uptime_ticks")
    if ticks is not None and ticks >= 0:
        # SNMP TimeTicks units: 1/100s.
        out["uptime_days"] = str(ticks // (100 * 86400))
    return out


async def read_snmpv3(config: dict[str, str]) -> dict[str, int]:
    from pysnmp.hlapi.v3arch.asyncio import (
        SnmpEngine, UsmUserData, UdpTransportTarget, ContextData,
        ObjectType, ObjectIdentity, get_cmd,
        usmHMACSHAAuthProtocol, usmAesCfb128Protocol, usmDESPrivProtocol,
    )

    engine = SnmpEngine()
    try:
        target = await UdpTransportTarget.create((NAS_HOST, 161), timeout=1, retries=0)
        auth = UsmUserData(
            userName=config["username"],
            authKey=config["auth_password"],
            privKey=config["priv_password"],
            authProtocol=usmHMACSHAAuthProtocol,
            privProtocol=(usmDESPrivProtocol if config["privacy_protocol"] == "des"
                          else usmAesCfb128Protocol),
        )
        error, error_status, _, varbinds = await get_cmd(
            engine, auth, target, ContextData(),
            *(ObjectType(ObjectIdentity(oid)) for oid in OIDS.values()),
        )
        if error or error_status:
            return {}
        values = {}
        for key, (_, value) in zip(OIDS, varbinds):
            try:
                values[key] = int(value)
            except Exception:
                continue
        return values
    finally:
        engine.close_dispatcher()


class NasMonitor:
    def __init__(self, ttl: float = TTL_SECONDS):
        self.ttl = ttl
        self._lock = threading.Lock()
        self._time = float("-inf")
        self._cache = empty_snapshot()

    def _poll(self) -> dict[str, str]:
        result = empty_snapshot()
        if not reachable():
            result["snmp_state"] = "unavailable"
            return result
        result["nas_state"] = "online"
        try:
            config = load_snmp_config()
        except (OSError, ValueError, json.JSONDecodeError):
            result["snmp_state"] = "config_error"
            return result
        if config is None:
            return result
        try:
            metrics = asyncio.run(asyncio.wait_for(read_snmpv3(config), timeout=3))
        except (OSError, ImportError, ValueError, RuntimeError, asyncio.TimeoutError):
            metrics = {}
        if not metrics:
            result["snmp_state"] = "unavailable"
            return result
        result["snmp_state"] = "ok"
        result.update(convert_metrics(metrics))
        return result

    def snapshot(self) -> dict[str, str]:
        now = time.monotonic()
        with self._lock:
            if now - self._time >= self.ttl:
                self._cache = self._poll()
                self._time = time.monotonic()
            return dict(self._cache)


nas_monitor = NasMonitor()


def snapshot_bytes() -> bytes:
    sample = nas_monitor.snapshot()
    return "".join(f"{key}={sample[key]}\n" for key in KEYS).encode("ascii")
