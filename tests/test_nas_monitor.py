import json, os, sys, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "server"))
import nas_monitor as mon

class NasTests(unittest.TestCase):
    def test_no_configuration(self):
        with patch.object(mon, "reachable", return_value=True), patch.object(mon, "load_snmp_config", return_value=None):
            s=mon.NasMonitor(ttl=0).snapshot()
        self.assertEqual((s["nas_state"],s["snmp_state"],s["raid"]),("online","not_configured","--"))
    def test_offline(self):
        with patch.object(mon, "reachable", return_value=False):
            s=mon.NasMonitor().snapshot()
        self.assertEqual(s["nas_state"],"offline")
    def test_metric_conversion(self):
        s=mon.convert_metrics({"cpu_pct":25,"mem_total":8000,"mem_free":2000,"temp_cpu_c":45,"uptime_ticks":17_280_000})
        self.assertEqual((s["cpu_pct"],s["ram_pct"],s["uptime_days"]),("25","75","2"))
        self.assertEqual(mon.convert_metrics({"cpu_pct":999,"mem_total":0,"temp_system_c":1000}),{})
    def test_config_mode(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"secret.json"
            path.write_text(json.dumps({"username":"infotab","auth_password":"example-not-real","priv_password":"different-example-not-real"}))
            os.chmod(path,0o644)
            with self.assertRaises(ValueError): mon.load_snmp_config(path)
            os.chmod(path,0o600)
            self.assertEqual(mon.load_snmp_config(path)["username"],"infotab")
    def test_contract(self):
        self.assertEqual(len(mon.KEYS),12)
        self.assertEqual(tuple(mon.empty_snapshot()),mon.KEYS)

if __name__=="__main__": unittest.main()
