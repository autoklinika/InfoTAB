#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

// InfoTAB v0.2 — laboratory touchscreen + Wi-Fi scan.
// The ESP32-P4 has no Wi-Fi radio. Tab5 connects to its onboard
// ESP32-C6 via SDIO. Pin mapping is from the official M5Stack Tab5 example.
// NO Wi-Fi password, network join, OTA or UPS commands in this firmware.

namespace {
constexpr gpio_num_t kSdioClk = GPIO_NUM_12;
constexpr gpio_num_t kSdioCmd = GPIO_NUM_13;
constexpr gpio_num_t kSdioD0 = GPIO_NUM_11;
constexpr gpio_num_t kSdioD1 = GPIO_NUM_10;
constexpr gpio_num_t kSdioD2 = GPIO_NUM_9;
constexpr gpio_num_t kSdioD3 = GPIO_NUM_8;
constexpr gpio_num_t kSdioRst = GPIO_NUM_15;

constexpr int kLeft = 34;
constexpr int kRight = 650;
constexpr int kScanButtonWidth = 430;
constexpr int kScanButtonHeight = 80;
constexpr uint32_t kTouchRefreshMs = 100;
constexpr uint32_t kLogUptimeMs = 10000;

int screenWidth = 0;
int screenHeight = 0;
int scanButtonX = 0;
int scanButtonY = 0;
uint32_t lastTouchRefresh = 0;
uint32_t lastUptimeLog = 0;
int lastTouchX = -1;
int lastTouchY = -1;
bool scanActive = false;
bool wifiInitialized = false;

void writeText(int x, int y, const String& message, uint16_t color, int scale = 2) {
    M5.Display.setTextColor(color, TFT_BLACK);
    M5.Display.setTextSize(scale);
    M5.Display.setCursor(x, y);
    M5.Display.print(message);
}

void drawScanButton() {
    const uint16_t bg = scanActive ? TFT_DARKGREY : TFT_DARKCYAN;
    M5.Display.fillRoundRect(scanButtonX, scanButtonY, kScanButtonWidth,
                             kScanButtonHeight, 14, bg);
    M5.Display.drawRoundRect(scanButtonX, scanButtonY, kScanButtonWidth,
                             kScanButtonHeight, 14, TFT_WHITE);
    M5.Display.setTextColor(TFT_WHITE, bg);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(scanButtonX + 42, scanButtonY + 22);
    M5.Display.print(scanActive ? "SKANOWANIE..." : "SKANUJ WI-FI");
}

void drawBaseScreen() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.fillRect(0, 0, screenWidth, 94, TFT_NAVY);
    M5.Display.setTextColor(TFT_CYAN, TFT_NAVY);
    M5.Display.setTextSize(4);
    M5.Display.setCursor(kLeft, 25);
    M5.Display.print("InfoTAB  v0.2");

    M5.Display.drawFastVLine(kRight - 24, 111, screenHeight - 132, TFT_DARKGREY);
    writeText(kLeft, 132, "TEST DOTYKU", TFT_GREEN, 3);
    writeText(kLeft, 182, "Dotknij dowolnego miejsca", TFT_WHITE);
    writeText(kRight, 132, "DIAGNOSTYKA WI-FI", TFT_CYAN, 3);
    writeText(kRight, 181, "ESP32-C6 / SDIO", TFT_LIGHTGREY);

    M5.Display.drawRect(kLeft, 255, 540, 290, TFT_DARKGREY);
    writeText(kLeft + 18, 268, "Pole podgladu dotyku", TFT_LIGHTGREY);
    M5.Display.drawRect(kRight, 244, screenWidth - kRight - 38,
                        screenHeight - 375, TFT_DARKGREY);

    scanButtonX = screenWidth - kScanButtonWidth - 42;
    scanButtonY = screenHeight - kScanButtonHeight - 35;
    drawScanButton();
    writeText(kLeft, screenHeight - 73, "USB COM6 | 115200 baud", TFT_LIGHTGREY);
    writeText(kLeft, screenHeight - 43, "Bez laczenia z siecia", TFT_YELLOW);
}

void showTouch(int x, int y, bool pressed) {
    M5.Display.fillRect(kLeft + 5, 298, 530, 235, TFT_BLACK);
    writeText(kLeft + 18, 314, "X: " + String(x), TFT_WHITE, 3);
    writeText(kLeft + 18, 364, "Y: " + String(y), TFT_WHITE, 3);
    writeText(kLeft + 18, 419, pressed ? "DOTYK: AKTYWNY" : "DOTYK: ZWOLNIONY",
              pressed ? TFT_GREEN : TFT_YELLOW);
    if (x >= 0 && y >= 0 && screenWidth > 0 && screenHeight > 0) {
        int px = kLeft + 18 + (x * 500 / screenWidth);
        int py = 471 + (y * 42 / screenHeight);
        M5.Display.fillCircle(px, py, 7, TFT_CYAN);
    }
}

void showWifiMessage(const String& text, uint16_t color = TFT_WHITE) {
    M5.Display.fillRect(kRight + 6, 252, screenWidth - kRight - 50,
                        screenHeight - 390, TFT_BLACK);
    writeText(kRight + 22, 267, text, color);
}

void scanWifi() {
    scanActive = true;
    drawScanButton();
    showWifiMessage("Skanowanie sieci...", TFT_YELLOW);
    Serial.println("[InfoTAB] Wi-Fi scan requested");
    if (!wifiInitialized) {
        // ESP32-P4 requires explicit SDIO pin mapping to its ESP32-C6 co-processor.
        WiFi.setPins(kSdioClk, kSdioCmd, kSdioD0, kSdioD1,
                     kSdioD2, kSdioD3, kSdioRst);
        if (!WiFi.mode(WIFI_STA)) {
            Serial.println("[InfoTAB] Wi-Fi mode initialization FAILED");
            showWifiMessage("Blad inicjalizacji Wi-Fi", TFT_RED);
            scanActive = false;
            drawScanButton();
            return;
        }
        wifiInitialized = true;
    }

    const int count = WiFi.scanNetworks();
    if (count < 0) {
        Serial.printf("[InfoTAB] Wi-Fi scan error: %d\n", count);
        showWifiMessage("Blad skanowania Wi-Fi", TFT_RED);
    } else if (count == 0) {
        Serial.println("[InfoTAB] Wi-Fi scan: 0 networks");
        showWifiMessage("Nie znaleziono sieci", TFT_YELLOW);
    } else {
        M5.Display.fillRect(kRight + 6, 252, screenWidth - kRight - 50,
                            screenHeight - 390, TFT_BLACK);
        writeText(kRight + 20, 266, "Znaleziono: " + String(count), TFT_GREEN);
        Serial.printf("[InfoTAB] Wi-Fi scan found %d network(s)\n", count);
        const int visible = min(count, 7);
        for (int i = 0; i < visible; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) ssid = "<ukryta>";
            if (ssid.length() > 20) ssid = ssid.substring(0, 20);
            const int rssi = WiFi.RSSI(i);
            writeText(kRight + 20, 304 + i * 40,
                      ssid + "  " + String(rssi) + " dBm", TFT_WHITE);
            Serial.printf("[InfoTAB] Wi-Fi #%d RSSI=%d dBm\n", i + 1, rssi);
        }
        if (count > visible) {
            writeText(kRight + 20, 304 + visible * 40,
                      "Pozostale: " + String(count - visible), TFT_LIGHTGREY);
        }
    }
    WiFi.scanDelete();
    scanActive = false;
    drawScanButton();
}

bool withinScanButton(int x, int y) {
    return x >= scanButtonX && x < (scanButtonX + kScanButtonWidth) &&
           y >= scanButtonY && y < (scanButtonY + kScanButtonHeight);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    // Tab5 panel revisions can differ in their default orientation.
    // Keep the diagnostics UI in landscape (wider than tall).
    if (M5.Display.width() < M5.Display.height()) {
        M5.Display.setRotation(0);
    }
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    drawBaseScreen();

    Serial.println("\n[InfoTAB] v0.2 touchscreen + Wi-Fi diagnostics");
    Serial.printf("[InfoTAB] LCD: %d x %d\n", screenWidth, screenHeight);
    Serial.printf("[InfoTAB] Touch enabled: %s\n",
                  M5.Touch.isEnabled() ? "YES" : "NO");
    Serial.println("[InfoTAB] Tap SCAN Wi-Fi to initialize ESP32-C6 SDIO.");
}

void loop() {
    M5.update();
    const auto touch = M5.Touch.getDetail();
    const uint32_t now = millis();

    if (touch.isPressed()) {
        if (touch.x != lastTouchX || touch.y != lastTouchY ||
            now - lastTouchRefresh >= kTouchRefreshMs) {
            showTouch(touch.x, touch.y, true);
            lastTouchRefresh = now;
            lastTouchX = touch.x;
            lastTouchY = touch.y;
        }
        if (touch.wasPressed()) {
            Serial.printf("[InfoTAB] Touch DOWN x=%d y=%d\n", touch.x, touch.y);
        }
    }
    if (touch.wasClicked()) {
        Serial.printf("[InfoTAB] Touch CLICK x=%d y=%d\n", touch.x, touch.y);
        showTouch(touch.x, touch.y, false);
        if (!scanActive && withinScanButton(touch.x, touch.y)) {
            scanWifi();  // diagnostic only, no credentials required
        }
    }

    if (now - lastUptimeLog >= kLogUptimeMs) {
        lastUptimeLog = now;
        Serial.printf("[InfoTAB] uptime=%lu s\n",
                      static_cast<unsigned long>(now / 1000));
    }
    delay(8);
}
