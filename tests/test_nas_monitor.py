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
    def test_qnap_disk_volume_fan_statuses(self):
        sample={"disk_slots":5,"volume_count":2,"fan_slots":2,
                "disk_status_1":0,"disk_status_2":0,"disk_status_3":0,
                "disk_status_4":0,"disk_status_5":-5,
                "volume_status_1":"Ready","volume_status_2":"Ready",
                "volume_free_kib_1":286912884,
                "volume_free_kib_2":7566636432,
                "fan_rpm_1":548,"fan_rpm_2":547}
        metrics=mon.convert_metrics(sample)
        self.assertEqual(metrics["disks"],"4/4 GOTOWE")
        self.assertEqual(metrics["volumes"],"2/2 READY")
        self.assertEqual(metrics["free_gib"],"7490")
        self.assertEqual(metrics["fan_rpm"],"548/547")
        self.assertNotIn("raid",metrics)
        sample["disk_status_4"]=-9
        self.assertEqual(mon.convert_metrics(sample)["disks"],"3/4 GOTOWE")
        sample["volume_status_2"]="Degraded"
        self.assertEqual(mon.convert_metrics(sample)["volumes"],"1/2 READY")
    def test_config_mode(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"secret.json"
            path.write_text(json.dumps({"username":"infotab","auth_password":"example-not-real","priv_password":"different-example-not-real"}))
            os.chmod(path,0o644)
            with self.assertRaises(ValueError): mon.load_snmp_config(path)
            os.chmod(path,0o600)
            self.assertEqual(mon.load_snmp_config(path)["username"],"infotab")
            self.assertEqual(mon.load_snmp_config(path)["privacy_protocol"],"des")
            cfg=json.loads(path.read_text()); cfg["privacy_protocol"]="aes"
            path.write_text(json.dumps(cfg)); os.chmod(path,0o600)
            self.assertEqual(mon.load_snmp_config(path)["privacy_protocol"],"aes")
            cfg["privacy_protocol"]="not-safe"
            path.write_text(json.dumps(cfg)); os.chmod(path,0o600)
            with self.assertRaises(ValueError): mon.load_snmp_config(path)
    def test_contract(self):
        self.assertEqual(len(mon.KEYS),13)
        self.assertEqual(tuple(mon.empty_snapshot()),mon.KEYS)

if __name__=="__main__": unittest.main()
