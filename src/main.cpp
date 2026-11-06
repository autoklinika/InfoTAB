#include <Arduino.h>
#include <M5Unified.h>

// InfoTAB v0.1: bezpieczny test LCD i portu szeregowego.
// Na razie bez konfiguracji sieci, danych UPS i aktualizacji OTA.

namespace {
constexpr uint32_t kStatusRefreshMs = 1000;
uint32_t lastStatusRefresh = 0;

void drawHeader() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(4);
    M5.Display.setCursor(40, 50);
    M5.Display.println("InfoTAB");

    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(40, 135);
    M5.Display.println("M5Stack Tab5 | ESP32-P4");
    M5.Display.setCursor(40, 175);
    M5.Display.println("Windows 11 / PlatformIO / COM6");

    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(40, 235);
    M5.Display.println("USB firmware: uruchomiony");

    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setCursor(40, 285);
    M5.Display.println("UPS / Wi-Fi / OTA: kolejne etapy");
}
}  // namespace

void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    drawHeader();

    Serial.println();
    Serial.println("[InfoTAB] Boot OK");
    Serial.printf("[InfoTAB] LCD: %d x %d\n",
                  M5.Display.width(), M5.Display.height());
}

void loop() {
    M5.update();
    const uint32_t now = millis();
    if (now - lastStatusRefresh >= kStatusRefreshMs) {
        lastStatusRefresh = now;
        Serial.printf("[InfoTAB] uptime=%lus\n",
                      static_cast<unsigned long>(now / 1000));
    }
    delay(10);
}
