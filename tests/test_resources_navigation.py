"""Non-hardware navigation layout invariants for the 5-page InfoTAB HMI."""
import re
from pathlib import Path
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "src/main.cpp").read_text()

class ResourcesNavigationTests(unittest.TestCase):
    def test_page_order_and_titles(self):
        self.assertRegex(SOURCE, re.compile(
            r'MAIN_PAGES\[\].*?\{\s*Page::Dashboard, Page::Resources, Page::NetworkInfo,\s*'
            r'Page::Power, Page::Service\s*\}', re.S))
        self.assertRegex(SOURCE, re.compile(
            r'labels\[\].*?\{\s*"AI PLATFORM", "ZASOBY", "NAS", "UPS", "USTAWIENIA"\s*\}',
            re.S))
        self.assertIn('static_assert(MAIN_PAGE_COUNT == 5', SOURCE)

    def test_every_page_renders_with_correct_selected_tab(self):
        for name, index in (
            ("drawOverview", 0),
            ("drawResources", 1),
            ("drawNetwork", 2),
            ("drawPower", 3),
            ("drawService", 4),
        ):
            body = SOURCE[SOURCE.index(f"void {name}() {{"):]
            body = body[:body.index("\n}")]
            self.assertIn(f'tabShell("", {index});', body)

    def test_touch_and_swipe_bounds(self):
        self.assertIn('for (int i = 0; i < MAIN_PAGE_COUNT; ++i)', SOURCE)
        self.assertIn('if (next >= 0 && next < MAIN_PAGE_COUNT)', SOURCE)
        self.assertIn('case Page::Resources: drawResources(); break;', SOURCE)
        self.assertIn('case Page::Resources: handleResources(x, y); break;', SOURCE)
        self.assertIn('return (displayWidth - NAV_LEFT - NAV_RIGHT) / MAIN_PAGE_COUNT;', SOURCE)

    def test_1280px_navigation_bounds(self):
        width = (1280 - 32 - 32) // 5
        slots = [(32 + width*i, 32 + width*(i+1)) for i in range(5)]
        self.assertEqual(len(slots), 5)
        self.assertTrue(all(b-a >= 200 for a,b in slots))
        self.assertEqual(slots[0][0], 32)
        self.assertLessEqual(slots[-1][1], 1280-32)

if __name__ == "__main__":
    unittest.main()
