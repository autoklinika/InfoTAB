#!/usr/bin/env python3
"""Read-only QNAP NAS monitoring for InfoTAB.

No QTS credentials, SNMP passwords or writes appear in HTTP output.
SNMPv3 configuration is stored only in a local 0600 JSON file.
Unknown measurements are returned as '--' rather than fabricated.
"""
from __future__ import annotations

import asyncio
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import socket
import threading
import time

NAS_HOST = "192.168.1.101"
CONFIG_PATH = Path.home() / ".config/infotab/nas-snmp.json"
TTL_SECONDS = 20.0
STAGE_K_STATUS_ROOT = Path("/srv/ai-data/platform/backup-status/stage-k")

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
    "disk_slots": "1.3.6.1.4.1.24681.1.3.10.0",
    "fan_slots": "1.3.6.1.4.1.24681.1.3.14.0",
    "volume_count": "1.3.6.1.4.1.24681.1.3.16.0",
    "raid_count": "1.3.6.1.4.1.24681.1.4.1.1.1.2.1.1.0",
}
KEYS = (
    "nas_state", "snmp_state", "cpu_pct", "ram_pct", "temp_cpu_c",
    "temp_system_c", "disks", "raid", "volumes", "free_gib", "fan_rpm",
    "uptime_days", "backup",
)


def empty_snapshot() -> dict[str, str]:
    return {
        "nas_state": "offline",
        "snmp_state": "not_configured",
        "cpu_pct": "--", "ram_pct": "--",
        "temp_cpu_c": "--", "temp_system_c": "--",
        "disks": "--", "raid": "--", "volumes": "--", "free_gib": "--",
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


def convert_metrics(values: dict[str, int | str]) -> dict[str, str]:
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
    if isinstance(ticks, int) and ticks >= 0:
        # QNAP system uptime (different from SNMP agent uptime), TimeTicks = 1/100 s.
        out["uptime_days"] = str(ticks // (100 * 86400))

    # QNAP hdStatusEX: ready=0, noDisk=-5, invalid=-6, rwError=-9, unknown=-4.
    # 'READY' does not imply SMART is healthy; never claim an exhaustive SMART test.
    slots = values.get("disk_slots")
    if isinstance(slots, int) and 0 < slots <= 16:
        states = [values.get(f"disk_status_{i}") for i in range(1, slots + 1)]
        if all(isinstance(v, int) for v in states):
            inserted = [v for v in states if v != -5]
            if inserted:
                ready = sum(v == 0 for v in inserted)
                out["disks"] = f"{ready}/{len(inserted)} GOTOWE"

    volumes = values.get("volume_count")
    if isinstance(volumes, int) and 0 < volumes <= 16:
        statuses = [values.get(f"volume_status_{i}") for i in range(1, volumes + 1)]
        if all(isinstance(v, str) and v.strip() for v in statuses):
            ready = sum(v.strip().casefold() == "ready" for v in statuses)
            out["volumes"] = f"{ready}/{volumes} READY"

        free_kib = [values.get(f"volume_free_kib_{i}") for i in range(1, volumes + 1)]
        if all(isinstance(v, int) and v >= 0 for v in free_kib):
            # This particular QTS NAS returned KiB despite MIB's 'byte' description.
            # Verified against textual legacy QTS values (273.62 GB / 7.05 TB).
            out["free_gib"] = str(round(sum(free_kib) / (1024 * 1024)))

    raids = values.get("raid_count")
    if isinstance(raids, int) and 0 < raids <= 16:
        statuses = [values.get(f"raid_status_{i}") for i in range(1, raids + 1)]
        if all(isinstance(v, str) and v.strip() for v in statuses):
            ready = sum(v.strip().casefold() == "ready" for v in statuses)
            out["raid"] = f"{ready}/{raids} READY"

    fans = values.get("fan_slots")
    if isinstance(fans, int) and 0 < fans <= 8:
        rpm = [values.get(f"fan_rpm_{i}") for i in range(1, fans + 1)]
        if all(isinstance(v, int) and 0 <= v <= 30000 for v in rpm):
            out["fan_rpm"] = "/".join(str(x) for x in rpm)
    return out


def read_ai_backup_state(
    status_root: Path = STAGE_K_STATUS_ROOT,
    now: datetime | None = None,
) -> str:
    """State of AI Platform Stage K backups ON GlobalNAS, not QNAP HBS jobs.

    Refuse PASS when a monitor is stale, daily/weekly reports are missing or
    unsuccessful. Only read monitor outputs; do not mount or access the NAS.
    """
    now = now or datetime.now(timezone.utc)
    def report(filename: str) -> dict[str, object] | None:
        path = status_root / filename
        try:
            if path.stat().st_size > 65536:
                return None
            obj = json.loads(path.read_text(encoding="utf-8"))
            return obj if isinstance(obj, dict) else None
        except (OSError, ValueError, UnicodeError):
            return None

    def recent(value: object, max_age_hours: float) -> bool:
        if not isinstance(value, str):
            return False
        try:
            dt = datetime.fromisoformat(value.replace("Z", "+00:00"))
            if dt.tzinfo is None:
                return False
            hours = (now - dt).total_seconds() / 3600
            return -0.1 <= hours <= max_age_hours
        except ValueError:
            return False

    monitor = report("monitor.json")
    daily = report("daily.json")
    weekly = report("weekly.json")
    if not all((monitor, daily, weekly)):
        return "--"

    # Any explicit FAIL should override historical successful backups.
    if any(d.get("status") != "PASS" for d in (monitor, daily, weekly)):
        return "AI STAGE K: ALARM"

    if monitor.get("issues"):
        return "AI STAGE K: ALARM"

    if not (recent(monitor.get("checked_at"), 3)
            and recent(daily.get("completed_at"), 36)
            and recent(weekly.get("completed_at"), 24 * 8)):
        return "AI STAGE K: NIEAKTUALNY"
    return "AI STAGE K: OK"


async def read_snmpv3(config: dict[str, str]) -> dict[str, int]:
    from pysnmp.hlapi.v3arch.asyncio import (
        SnmpEngine, UsmUserData, UdpTransportTarget, ContextData,
        ObjectType, ObjectIdentity, get_cmd,
        usmHMACSHAAuthProtocol, usmAesCfb128Protocol, usmDESPrivProtocol,
    )

    engine = SnmpEngine()
    try:
        # QNAP SHA/DES SNMPv3 discovery may exceed 1s on a fresh engine.
        target = await UdpTransportTarget.create((NAS_HOST, 161), timeout=2.5, retries=0)
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
        values: dict[str, int | str] = {}
        for key, (_, value) in zip(OIDS, varbinds):
            try:
                values[key] = int(value)
            except (ValueError, TypeError):
                continue

        # Read only fixed, known health/status and counter OIDs. Never fetch
        # serial numbers, share names, disk models, or QTS credentials.
        extended: dict[str, str] = {}
        for i in range(1, min(values.get("disk_slots", 0), 16) + 1):
            extended[f"disk_status_{i}"] = f"1.3.6.1.4.1.24681.1.3.11.1.4.{i}"
        for i in range(1, min(values.get("volume_count", 0), 16) + 1):
            extended[f"volume_free_kib_{i}"] = f"1.3.6.1.4.1.24681.1.3.17.1.5.{i}"
            extended[f"volume_status_{i}"] = f"1.3.6.1.4.1.24681.1.3.17.1.6.{i}"
        for i in range(1, min(values.get("fan_slots", 0), 8) + 1):
            extended[f"fan_rpm_{i}"] = f"1.3.6.1.4.1.24681.1.3.15.1.3.{i}"
        for i in range(1, min(values.get("raid_count", 0), 16) + 1):
            extended[f"raid_status_{i}"] = f"1.3.6.1.4.1.24681.1.4.1.1.1.2.1.2.1.5.{i}"

        if extended:
            err2, status2, _, rows = await get_cmd(
                engine, auth, target, ContextData(),
                *(ObjectType(ObjectIdentity(oid)) for oid in extended.values()),
            )
            if not err2 and not status2:
                for key, (_, value) in zip(extended, rows):
                    if key.startswith(("volume_status_", "raid_status_")):
                        # Only bounded status strings; no volume labels or RAID identifiers.
                        raw = str(value).strip()
                        if raw and len(raw) <= 64:
                            values[key] = raw
                    else:
                        try:
                            values[key] = int(value)
                        except (ValueError, TypeError):
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
        result["backup"] = read_ai_backup_state()
        try:
            config = load_snmp_config()
        except (OSError, ValueError, json.JSONDecodeError):
            result["snmp_state"] = "config_error"
            return result
        if config is None:
            return result
        try:
            metrics = asyncio.run(asyncio.wait_for(read_snmpv3(config), timeout=5.5))
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
