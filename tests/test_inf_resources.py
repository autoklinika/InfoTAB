"""InfoTAB host resource telemetry: real contract and graceful degradation."""
import ipaddress
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import urlopen
from http.server import ThreadingHTTPServer
import threading
import sys
import os

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "server"))
import infotab_resources as r
import infotab_services as gateway


class ResourcesTests(unittest.TestCase):
    def test_fixed_keys_and_no_credentials(self):
        self.assertEqual(len(r.KEYS), 19)
        self.assertEqual(tuple(r.unknown()), r.KEYS)
        self.assertTrue(all(v == "--" for v in r.unknown().values()))
        self.assertTrue(set(r.KEYS).isdisjoint({
            "password", "token", "serial", "username",
        }))

    def test_proc_meminfo(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"meminfo"
            path.write_text("MemTotal:        32768 kB\nMemAvailable:    24576 kB\n")
            self.assertEqual(r.proc_meminfo(path), (8, 32))
            path.write_text("MemTotal: 32768 kB\nMemAvailable: 65536 kB\n")
            self.assertIsNone(r.proc_meminfo(path))

    def test_cpu_stat(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"stat"
            path.write_text("cpu  1 2 3 4 5 6 7 8 9 10\n")
            self.assertEqual(r.cpu_counters(path), (55, 9))

    def test_temperature_range(self):
        self.assertEqual(r.safe_temperature_milli(29500), "30")
        self.assertEqual(r.safe_temperature_milli(900000), "--")

    def test_nvidia_selects_5060_ti_even_if_not_gpu_zero(self):
        class Result:
            returncode = 0
            stdout = (
                "0, NVIDIA L40, 41, 35, 1024, 49140, 90.0, N/A\n"
                "1, NVIDIA GeForce RTX 5060 Ti, 35, 3, 2, 16311, 3.5, 0\n"
            )
        out = r.unknown()
        with patch.object(r.subprocess, "run", return_value=Result()):
            r.read_nvidia(out)
        self.assertEqual(out["rtx_temp_c"], "35")
        self.assertEqual(out["rtx_vram_total_mib"], "16311")
        self.assertEqual(out["rtx_fan_pct"], "0")

    def test_gateway_denies_non_lan_ip(self):
        previous=gateway.ALLOWED_LAN
        gateway.ALLOWED_LAN=ipaddress.ip_network("192.168.1.0/24")
        srv=ThreadingHTTPServer(("127.0.0.1",0),gateway.Handler)
        th=threading.Thread(target=srv.serve_forever,daemon=True)
        try:
            th.start()
            with self.assertRaises(HTTPError) as e:
                urlopen(f"http://127.0.0.1:{srv.server_port}/resources",timeout=5)
            self.assertEqual(e.exception.code,403)
            e.exception.close()
        finally:
            srv.shutdown()
            srv.server_close()
            gateway.ALLOWED_LAN=previous

    def test_monitor_cache(self):
        counts={"cpu":0}
        def cpu_fn(dest):
            counts["cpu"]+=1
            dest["cpu_temp_c"]="31"
        with patch.object(r,"read_cpu_memory_nvme",side_effect=cpu_fn),\
             patch.object(r,"read_radeon",side_effect=lambda d:None),\
             patch.object(r,"read_nvidia",side_effect=lambda d:None):
            m=r.ResourceMonitor(ttl=5)
            self.assertEqual(m.snapshot()["cpu_temp_c"],"31")
            self.assertEqual(m.snapshot()["cpu_temp_c"],"31")
            self.assertEqual(counts["cpu"],1)
            self.assertEqual(m.snapshot()["rtx_temp_c"],"--")

    def test_gateway_resources_and_existing_routes(self):
        previous=gateway.ALLOWED_LAN
        gateway.ALLOWED_LAN=ipaddress.ip_network("127.0.0.0/8")
        srv=ThreadingHTTPServer(("127.0.0.1",0),gateway.Handler)
        th=threading.Thread(target=srv.serve_forever,daemon=True)
        try:
            th.start()
            base=f"http://127.0.0.1:{srv.server_port}"
            with urlopen(base+"/resources",timeout=10) as response:
                data=response.read().decode("ascii")
                self.assertEqual(response.status,200)
                self.assertTrue(data.startswith("cpu_temp_c="))
                self.assertEqual(len(data.strip().splitlines()),19)
            with urlopen(base+"/services",timeout=10) as response:
                self.assertEqual(response.status,200)
            with urlopen(base+"/nas",timeout=10) as response:
                self.assertEqual(response.status,200)
            with self.assertRaises(HTTPError) as e:
                urlopen(base+"/foobar",timeout=4)
            self.assertEqual(e.exception.code,404)
            e.exception.close()
        finally:
            srv.shutdown()
            srv.server_close()
            gateway.ALLOWED_LAN=previous


if __name__=="__main__":
    unittest.main()
