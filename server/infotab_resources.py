#!/usr/bin/env python3
"""InfoTAB: read-only telemetry from the local AI Platform host.

Fixed-value allowlist, no credentials or arbitrary paths in requests.
Linux /proc + /sys and a narrowly-scoped nvidia-smi GPU query.
The Radeon VRAM is an allocation in shared/unified memory, NOT a
second independent physical RAM pool.
"""
from __future__ import annotations

from pathlib import Path
import re
import subprocess
import threading
import time

CACHE_TTL_SEC = 3.5

KEYS = (
    "cpu_temp_c", "cpu_load_pct", "cpu_freq_mhz",
    "ram_used_mib", "ram_total_mib",
    "radeon_temp_c", "radeon_load_pct", "radeon_vram_used_mib",
    "radeon_vram_total_mib", "radeon_power_w",
    "rtx_temp_c", "rtx_load_pct", "rtx_vram_used_mib",
    "rtx_vram_total_mib", "rtx_power_w", "rtx_fan_pct",
    "nvme1_temp_c", "nvme2_temp_c", "uptime_h",
)


def unknown() -> dict[str, str]:
    return {name: "--" for name in KEYS}


def text_file(path: Path, max_size: int = 65536) -> str:
    # /sys and /proc often report st_size=4096 or 0 regardless of
    # actual contents; enforce read limit on *bytes read*, not stat size.
    if not path.is_file():
        raise ValueError("Unsupported telemetry source")
    with path.open("r", encoding="ascii") as f:
        body = f.read(max_size + 1)
    if len(body) > max_size:
        raise ValueError("Telemetry source exceeds limit")
    return body.strip()


def integer_file(path: Path) -> int:
    return int(text_file(path, 128))


def hwmon_dir(sensor: str, hwmon_root: Path = Path("/sys/class/hwmon")) -> list[Path]:
    found = []
    for path in hwmon_root.glob("hwmon*"):
        try:
            if text_file(path / "name", 120) == sensor:
                found.append(path)
        except (OSError, ValueError, UnicodeError):
            continue
    return sorted(found, key=lambda p: str(p))


def safe_temperature_milli(value: int) -> str:
    c = value / 1000.0
    return str(round(c)) if -10 <= c <= 125 else "--"


def proc_meminfo(path: Path = Path("/proc/meminfo")) -> tuple[int, int] | None:
    values = {}
    for line in text_file(path).splitlines():
        if line.startswith(("MemTotal:", "MemAvailable:")):
            key, rest = line.split(":", 1)
            match = re.fullmatch(r"\s*(\d+)\s+kB\s*", rest)
            if match:
                values[key] = int(match.group(1))
    total, available = values.get("MemTotal"), values.get("MemAvailable")
    if not total or available is None or not (0 <= available <= total):
        return None
    # kB is 1024 bytes; MiB is 1024 KiB.
    return round((total - available) / 1024), round(total / 1024)


def cpu_counters(path: Path = Path("/proc/stat")) -> tuple[int, int] | None:
    line = text_file(path).splitlines()[0].split()
    if not line or line[0] != "cpu" or len(line) < 5:
        return None
    vals = list(map(int, line[1:]))
    total = sum(vals)
    idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
    return total, idle


def cpu_usage() -> str:
    try:
        a = cpu_counters()
        time.sleep(0.16)
        b = cpu_counters()
        if a is None or b is None:
            return "--"
        delta = b[0] - a[0]
        idle = b[1] - a[1]
        if delta <= 0 or idle < 0:
            return "--"
        return str(max(0, min(100, round(100 * (delta - idle) / delta))))
    except (OSError, ValueError, IndexError):
        return "--"


def average_cpu_mhz() -> str:
    points = []
    for path in Path("/sys/devices/system/cpu").glob("cpu[0-9]*/cpufreq/scaling_cur_freq"):
        try:
            khz = integer_file(path)
            if 100_000 <= khz <= 9_000_000:
                points.append(khz)
        except (OSError, ValueError):
            pass
    return str(round(sum(points) / len(points) / 1000)) if points else "--"


def radeon_device() -> Path | None:
    for path in Path("/sys/class/drm").glob("card[0-9]/device"):
        try:
            if text_file(path / "vendor", 30).lower() == "0x1002":
                return path
        except (OSError, ValueError):
            continue
    return None


def read_radeon(out: dict[str, str]) -> None:
    device = radeon_device()
    if device is None:
        return
    values = {
        "radeon_load_pct": ("gpu_busy_percent", 0, 100, 1),
        "radeon_vram_used_mib": ("mem_info_vram_used", 0, 2**47, 2**20),
        "radeon_vram_total_mib": ("mem_info_vram_total", 1, 2**47, 2**20),
    }
    for key, (filename, lo, hi, divisor) in values.items():
        try:
            val = integer_file(device / filename)
            if lo <= val <= hi:
                out[key] = str(round(val / divisor))
        except (OSError, ValueError):
            pass
    for hw in hwmon_dir("amdgpu"):
        try:
            out["radeon_temp_c"] = safe_temperature_milli(integer_file(hw / "temp1_input"))
        except (OSError, ValueError):
            pass
        try:
            microwatts = integer_file(hw / "power1_average")
            if 0 <= microwatts <= 400_000_000:
                out["radeon_power_w"] = f"{microwatts / 1_000_000:.1f}"
        except (OSError, ValueError):
            pass
        break


def read_cpu_memory_nvme(out: dict[str, str]) -> None:
    out["cpu_load_pct"] = cpu_usage()
    out["cpu_freq_mhz"] = average_cpu_mhz()
    try:
        k10 = hwmon_dir("k10temp")
        if k10:
            out["cpu_temp_c"] = safe_temperature_milli(
                integer_file(k10[0] / "temp1_input")
            )
    except (OSError, ValueError):
        pass
    try:
        memory = proc_meminfo()
        if memory:
            out["ram_used_mib"], out["ram_total_mib"] = map(str, memory)
    except (OSError, ValueError):
        pass
    for i, nvme in enumerate(hwmon_dir("nvme")[:2]):
        try:
            out[f"nvme{i + 1}_temp_c"] = safe_temperature_milli(
                integer_file(nvme / "temp1_input")
            )
        except (OSError, ValueError):
            pass
    try:
        uptime = float(text_file(Path("/proc/uptime")).split()[0])
        if 0 <= uptime <= 10**10:
            out["uptime_h"] = str(int(uptime // 3600))
    except (OSError, ValueError, IndexError):
        pass


def read_nvidia(out: dict[str, str]) -> None:
    query = [
        "/usr/bin/nvidia-smi",
        "--query-gpu=index,name,temperature.gpu,utilization.gpu,memory.used,"
        "memory.total,power.draw,fan.speed",
        "--format=csv,noheader,nounits",
    ]
    try:
        result = subprocess.run(
            query, capture_output=True, text=True, check=False, timeout=2.2,
            stdin=subprocess.DEVNULL, env={"PATH": "/usr/bin:/bin"},
        )
    except (OSError, subprocess.TimeoutExpired):
        return
    if result.returncode or not result.stdout:
        return
    for line in result.stdout.splitlines():
        columns = [x.strip() for x in line.split(",")]
        # Select the actual 5060 Ti by its name, not by mutable GPU index.
        # A second NVIDIA device may be added later.
        if len(columns) != 8 or "RTX 5060 Ti" not in columns[1]:
            continue
        values = columns[2:]
        names = (
            "rtx_temp_c", "rtx_load_pct", "rtx_vram_used_mib",
            "rtx_vram_total_mib", "rtx_power_w", "rtx_fan_pct",
        )
        for name, val in zip(names, values):
            try:
                if name == "rtx_power_w":
                    number = float(val)
                    if 0 <= number <= 500:
                        out[name] = f"{number:.1f}"
                else:
                    number = int(val)
                    hi = 100000 if "vram" in name else (125 if "temp" in name else 100)
                    if 0 <= number <= hi:
                        out[name] = str(number)
            except (ValueError, OverflowError):
                pass
        break


class ResourceMonitor:
    def __init__(self, ttl: float = CACHE_TTL_SEC) -> None:
        self.ttl = ttl
        self._lock = threading.Lock()
        self._cache = unknown()
        self._at = float("-inf")

    def snapshot(self) -> dict[str, str]:
        with self._lock:
            if time.monotonic() - self._at >= self.ttl:
                values = unknown()
                read_cpu_memory_nvme(values)
                read_radeon(values)
                read_nvidia(values)
                self._cache = values
                self._at = time.monotonic()
            return dict(self._cache)


resources_monitor = ResourceMonitor()


def snapshot_bytes() -> bytes:
    values = resources_monitor.snapshot()
    return "".join(f"{k}={values[k]}\n" for k in KEYS).encode("ascii")
