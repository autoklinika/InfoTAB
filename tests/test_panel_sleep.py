"""Source-level guards for M5Stack Tab5 LCD controller sleep (hardware test pending)."""
import re
import unittest
from pathlib import Path

SOURCE = (Path(__file__).resolve().parents[1] / "src/main.cpp").read_text()


class Tab5PanelSleepTests(unittest.TestCase):
    def test_real_display_off_not_sleep_in(self):
        self.assertIn("panelDisplayCommand(0x28)", SOURCE)
        self.assertIn("panelDisplayCommand(0x29)", SOURCE)
        self.assertNotIn("M5.Display.sleep();", SOURCE)
        self.assertNotIn("M5.Display.wakeup();", SOURCE)
        self.assertNotIn("M5.Power.deepSleep(", SOURCE)
        self.assertNotIn("esp_deep_sleep_start(", SOURCE)
        self.assertIn("PANEL_IDLE_SLEEP_MS = 5UL * 60UL * 1000UL", SOURCE)

    def test_dsi_bus_used_instead_of_noop_generic_command(self):
        self.assertIn('lgfx::bus_type_t::bus_dsi', SOURCE)
        self.assertIn('static_cast<lgfx::Panel_DSI*>(rawPanel)', SOURCE)
        self.assertIn('bus->writeParams(command, nullptr, 0)', SOURCE)
        self.assertNotIn('M5.Display.writeCommand(0x28)', SOURCE)
        self.assertIn('if (!panelDisplayCommand(0x28))', SOURCE)

    def test_backlight_restored_after_display_on(self):
        self.assertIn('panelSavedBrightness = M5.Display.getBrightness();', SOURCE)
        self.assertIn('M5.Display.setBrightness(0);', SOURCE)
        self.assertRegex(SOURCE, re.compile(
            r'bool displayOn = panelDisplayCommand\(0x29\).*?'
            r'M5.Display.setBrightness\(panelSavedBrightness\);', re.S
        ))
        self.assertIn('if (!displayOn) {', SOURCE)
        self.assertIn('panelPilotTouchFailed = true;', SOURCE)

    def test_input_wakes_before_normal_hmi_dispatch(self):
        sleeping = SOURCE.index("    if (panelAsleep) {\n        if (touch.wasPressed()")
        dispatch = SOURCE.index("    if (!handledSwipe && touch.wasClicked())")
        self.assertLess(sleeping, dispatch)
        self.assertIn("leavePanelSleep(true);", SOURCE)
        self.assertIn("ignoreWakeTouchUntilRelease = touchWake;", SOURCE)
        self.assertIn("if (ignoreWakeTouchUntilRelease) {", SOURCE)

    def test_sleep_only_on_dashboards_and_ota_disarmed(self):
        self.assertRegex(
            SOURCE,
            re.compile(
                r"if \(mainPage\(page\) && !otaArmed && !otaInProgress &&"
                r"\s*!panelPilotTouchFailed && !touch\.isPressed\(\)", re.S,
            ),
        )
        self.assertIn("millis() - panelLastActivityAt >= PANEL_IDLE_SLEEP_MS", SOURCE)

    def test_panel_variant_recovery_during_pilot_only(self):
        self.assertIn("PANEL_WAKE_PROBE_MS = 60UL * 1000UL", SOURCE)
        self.assertIn("!panelTouchWakeConfirmed", SOURCE)
        self.assertIn("leavePanelSleep(false);", SOURCE)
        self.assertIn("panelTouchWakeConfirmed = true;", SOURCE)
        self.assertIn("PANEL_MANUAL_WAKE_PROBE_MS = 20UL * 1000UL", SOURCE)
        self.assertIn("panelPilotTouchFailed = true;", SOURCE)
        self.assertIn("panelPilotTouchFailed = false;", SOURCE)

    def test_manual_sleep_is_accessible_and_deferred_until_release(self):
        self.assertIn('"WYLACZ EKRAN (TEST)"', SOURCE)
        self.assertIn('if (inside(x, y, 57, 513, 540, 50))', SOURCE)
        self.assertIn('panelManualSleepRequested = true;', SOURCE)
        self.assertRegex(SOURCE, re.compile(
            r'if \(panelManualSleepRequested && !touch\.isPressed\(\)'
            r' &&\s*!otaArmed && !otaInProgress\) \{'
            r'.*?enterPanelSleep\(true\);.*?return;', re.S,
        ))

    def test_visible_idle_and_last_wakeup_reason(self):
        self.assertIn('String((millis() - panelLastActivityAt) / 1000)', SOURCE)
        self.assertIn('panelWakeStatus = "DOTYK OK";', SOURCE)
        self.assertIn('panelWakeStatus = "AUTO / BRAK DOTYKU";', SOURCE)
        self.assertIn('InfoTAB v0.9.7', SOURCE)

    def test_wake_redraw_and_reacquire_values(self):
        self.assertRegex(
            SOURCE, re.compile(
                r"void leavePanelSleep\(bool touchWake\) \{.*?"
                r"panelDisplayCommand\(0x29\).*?render\(\);.*?"
                r"lastDashboardRefresh = 0;", re.S,
            ),
        )


if __name__ == "__main__":
    unittest.main()
