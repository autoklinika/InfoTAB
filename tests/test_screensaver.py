"""InfoTAB v0.9.8 animated saver: screen and touch invariants.

Hardware animation and display brightness still require a real Tab5
OTA acceptance test. These source-level checks prevent common regressions.
"""
import re
import unittest
from pathlib import Path

SRC = (Path(__file__).resolve().parents[1] / "src/main.cpp").read_text()


class AnimatedSaverTests(unittest.TestCase):
    def test_panel_never_sleeps(self):
        for forbidden in (
            "M5.Display.sleep()", "M5.Display.wakeup()",
            "esp_deep_sleep_start(", "M5.Power.deepSleep(",
            "panelDisplayCommand(0x28)", "panelDisplayCommand(0x10)",
        ):
            self.assertNotIn(forbidden, SRC)
        self.assertIn("SAVER_IDLE_MS = 5UL * 60UL * 1000UL", SRC)

    def test_motion_is_frame_limited_and_incremental(self):
        self.assertIn("SAVER_FRAME_MS = 45", SRC)
        self.assertIn("if (saverLastFrameAt && now - saverLastFrameAt < SAVER_FRAME_MS)", SRC)
        frame = SRC.split("void drawScreensaverFrame() {", 1)[1].split("\n}\n", 1)[0]
        self.assertNotIn("fillScreen(", frame)
        self.assertIn("saverEraseObject(saverPreviousX[i], saverPreviousY[i])", frame)
        self.assertIn("M5.Display.fillCircle(x, y, 5, SAVER_HEAD)", frame)
        self.assertIn("sinf(angle)", frame)

    def test_touch_exits_before_normal_ui_and_swallow_wake(self):
        wake = SRC.index("if (saverActive) {\n        // Arm wake only")
        handler = SRC.index("if (!handledSwipe && touch.wasClicked())")
        self.assertLess(wake, handler)
        self.assertIn("stopScreensaver();", SRC[wake:handler])
        self.assertIn("saverWakeInputArmed = false;", SRC)
        self.assertIn("if (!saverWakeInputArmed)", SRC)
        self.assertIn("saverWakeInputArmed = true;", SRC)
        self.assertIn("saverIgnoreWakeTap = true;", SRC)
        self.assertIn("if (saverIgnoreWakeTap)", SRC)
        self.assertIn("render();             // previous module restored", SRC)

    def test_brightness_is_gently_reduced_and_restored(self):
        self.assertIn("saverPreviousBrightness = M5.Display.getBrightness();", SRC)
        self.assertIn("saverBrightness", SRC)
        self.assertIn("M5.Display.setBrightness(saverBrightness);", SRC)
        self.assertIn("M5.Display.setBrightness(saverPreviousBrightness);", SRC)
        self.assertNotIn("M5.Display.setBrightness(0);", SRC)

    def test_manual_preview_and_auto_idle_are_ota_safe(self):
        self.assertIn('"URUCHOM WYGASZACZ"', SRC)
        self.assertIn('inside(x, y, 57, 519, 540, 48)', SRC)
        self.assertIn("if (!otaArmed && !otaInProgress) startScreensaver();", SRC)
        self.assertRegex(SRC, re.compile(
            r"if \(mainPage\(page\) && !otaArmed && !otaInProgress &&"
            r"\s*!touch\.isPressed\(\) &&"
            r"\s*millis\(\) - saverLastInteractionAt >= SAVER_IDLE_MS\)", re.S))
        self.assertIn("if (saverActive || otaArmed || otaInProgress) return;", SRC)

    def test_background_telemetry_kept_without_lcd_repaint(self):
        self.assertIn("SAVER_TELEMETRY_MS = 15UL * 1000UL", SRC)
        body = SRC.split("void pollModuleWhileScreensaver() {", 1)[1].split("\n}\n", 1)[0]
        for call in ("pollPlatformServices()", "pollResources()",
                     "pollNas()", "pollUpsReadOnly()"):
            self.assertIn(call, body)
        for forbidden in ("drawDashboardValues()", "render()", "fillScreen("):
            self.assertNotIn(forbidden, body)

    def test_five_modules_and_settings_still_present(self):
        self.assertIn("InfoTAB v0.9.8", SRC)
        self.assertIn('"AI PLATFORM", "ZASOBY", "NAS", "UPS", "USTAWIENIA"', SRC)
        self.assertIn("case Page::OtaUpload: handleOtaUpload(x, y); break;", SRC)


if __name__ == "__main__":
    unittest.main()
