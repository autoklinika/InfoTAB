#include <Arduino.h>
#include <cstring>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include "InfoTABModbus.h"
#include <esp_ota_ops.h>
#include <esp_partition.h>

// InfoTAB v0.6: direct Modbus TCP UPS + Wi-Fi OTA + differential LCD refresh,
// with existing touchscreen Wi-Fi setup and read-only OTA diagnostics.
// Credentials are NEVER embedded in source, printed on Serial, or sent to GitHub.
// The ESP32-P4 uses the ESP32-C6 Wi-Fi companion via M5Stack's SDIO pin mapping.

namespace {
constexpr gpio_num_t SDIO_CLK = GPIO_NUM_12;
constexpr gpio_num_t SDIO_CMD = GPIO_NUM_13;
constexpr gpio_num_t SDIO_D0  = GPIO_NUM_11;
constexpr gpio_num_t SDIO_D1  = GPIO_NUM_10;
constexpr gpio_num_t SDIO_D2  = GPIO_NUM_9;
constexpr gpio_num_t SDIO_D3  = GPIO_NUM_8;
constexpr gpio_num_t SDIO_RST = GPIO_NUM_15;

constexpr int MARGIN = 32;
constexpr int MAX_NETWORKS = 32;
constexpr int NETWORKS_PER_PAGE = 7;
constexpr int KEY_GAP = 9;
constexpr int KEY_HEIGHT = 51;
constexpr int KEY_FIRST_Y = 313;
constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t HOME_REFRESH_MS = 5000;
// Direct PowerWalker Modbus TCP is implemented in InfoTABModbus.h.
constexpr uint32_t DASH_REFRESH_MS = 8000;
constexpr const char* BRIDGE_HEALTH = "http://192.168.1.100:8080/health";

enum class Page { Dashboard, Power, NetworkInfo, Service, Home, Networks, Password, Connecting, OtaDiag, OtaUpload };
struct NetworkEntry {
    String ssid;
    int rssi = -100;
    bool secured = true;
};
NetworkEntry networks[MAX_NETWORKS];
int networkCount = 0;
int networkPage = 0;
int displayWidth = 1280;
int displayHeight = 720;

Page page = Page::Dashboard;
String infoMessage;
String savedSsid;
String savedPassword;
String pendingSsid;
String pendingPassword;
bool passwordSecure = true;
bool upperCase = false;
bool symbolMode = false;
bool wifiReady = false;
bool saveAfterSuccess = false;
uint32_t connectionStart = 0;
uint32_t lastHomeRefresh = 0;
uint32_t lastDashboardRefresh = 0;
bool aiBridgeOk = false;
bool aiBridgeChecked = false;
String aiBridgeVersion = "--";
String aiDatabaseState = "--";
String aiOllamaState = "--";
bool nasChecked = false;
bool nasHttps = false;
bool nasQts = false;
uint32_t nasLastSuccessAt = 0;


struct UpsData {
    bool valid = false;
    int modeCode = -1;
    int batteryPct = -1;
    int loadPct = -1;
    int inputVoltTenths = -1;
    int outputVoltTenths = -1;
    int outputHzTenths = -1;
    int batteryVoltTenths = -1;
    int32_t powerWattsEstimate = -1;
    long runtimeSeconds = -1;
    uint32_t updatedAt = 0;
};
UpsData ups;
bool otaArmed = false;
bool otaInProgress = false;
uint32_t otaArmedAt = 0;
constexpr uint32_t OTA_WINDOW_MS = 120000;
String otaStatus = "Gotowe do aktywacji aktualizacji.";

void logEvent(const char* event) {
    // Deliberately never log SSIDs or passwords.
    Serial.printf("[InfoTAB] %s\n", event);
}

bool inside(int x, int y, int bx, int by, int w, int h) {
    return x >= bx && x < bx + w && y >= by && y < by + h;
}

void writeText(int x, int y, const String& value, uint16_t color, int scale = 2) {
    M5.Display.setTextColor(color, 0x322A);
    M5.Display.setTextSize(scale);
    M5.Display.setCursor(x, y);
    M5.Display.print(value);
}

void button(int x, int y, int w, int h, const String& label,
            uint16_t bg = TFT_DARKCYAN, int scale = 2) {
    M5.Display.fillRoundRect(x, y, w, h, 12, bg);
    M5.Display.drawRoundRect(x, y, w, h, 12, TFT_LIGHTGREY);
    M5.Display.setTextColor(TFT_WHITE, bg);
    M5.Display.setTextSize(scale);
    M5.Display.setCursor(x + 14, y + (h - 16 * scale) / 2);
    M5.Display.print(label);
}

void header(const String& title) {
    M5.Display.fillScreen(0x322A);
    M5.Display.fillRect(0, 0, displayWidth, 90, 0x532E);
    M5.Display.setTextColor(0x8678, 0x532E);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(MARGIN, 22);
    M5.Display.print("InfoTAB");
    M5.Display.setTextColor(0xFFFF, 0x532E);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(MARGIN + 310, 34);
    M5.Display.print(title);
}

bool connected() {
    return wifiReady && WiFi.status() == WL_CONNECTED;
}


// Forward declaration: dashboard navigation uses render() below.
void render();

void pollUpsReadOnly() {
    UpsData snapshot;
    if (!connected()) { ups = snapshot; return; }
    infotab_modbus::Sample m;
    if (!infotab_modbus::poll(m)) {
        ups = snapshot;
        logEvent("MODBUS TCP UPS: no valid data");
        return;
    }
    snapshot.valid = true;
    snapshot.modeCode = m.modeCode;
    snapshot.batteryPct = m.batteryPercent;
    snapshot.loadPct = m.loadPercent;
    snapshot.inputVoltTenths = m.inputVolts10;
    snapshot.outputVoltTenths = m.outputVolts10;
    snapshot.outputHzTenths = m.outputHertz10;
    snapshot.batteryVoltTenths = m.batteryVolts10;
    snapshot.powerWattsEstimate = m.outputActiveWattsEstimate;
    snapshot.runtimeSeconds = m.runtimeSeconds;
    snapshot.updatedAt = millis();
    ups = snapshot;
    Serial.printf("[InfoTAB] Modbus UPS mode=%d battery=%d%% load=%d%% voltage=%d/10V runtime=%lds\n",
                  ups.modeCode, ups.batteryPct, ups.loadPct,
                  ups.outputVoltTenths, ups.runtimeSeconds);
}

// Extract a flat string property from the public AI Bridge /health JSON.
// No credentials or extra dependencies. Unknown fields remain "--".
String healthString(const String& body, const char* key) {
    const String name = String("\"") + key + "\"";
    int pos = body.indexOf(name);
    if (pos < 0) return "--";
    pos = body.indexOf(':', pos + name.length());
    if (pos < 0) return "--";
    pos = body.indexOf('"', pos + 1);
    if (pos < 0) return "--";
    int stop = body.indexOf('"', pos + 1);
    if (stop < 0 || stop - pos > 40) return "--";
    return body.substring(pos + 1, stop);
}
void pollAiBridge() {
    aiBridgeChecked = true;
    aiBridgeOk = false;
    aiDatabaseState = "--";
    aiOllamaState = "--";
    aiBridgeVersion = "--";
    if (!connected()) return;
    NetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(1000);
    http.setTimeout(1400);
    if (http.begin(client, String(BRIDGE_HEALTH))) {
        const int status = http.GET();
        aiBridgeOk = status == 200;
        if (aiBridgeOk) {
            const String payload = http.getString();
            aiBridgeVersion = healthString(payload, "version");
            aiDatabaseState = healthString(payload, "database");
            aiOllamaState = healthString(payload, "ollama");
        }
        http.end();
    }
}

// Only reachability checks. Do not pretend they are QNAP storage/backup health.
void pollNas() {
    nasChecked = true;
    nasHttps = false;
    nasQts = false;
    if (!connected()) return;
    NetworkClient client;
    nasHttps = client.connect("192.168.1.101", 443, 850);
    client.stop();
    nasQts = client.connect("192.168.1.101", 8080, 850);
    client.stop();
    if (nasHttps || nasQts) nasLastSuccessAt = millis();
}

// Static screen elements are drawn only when navigating to this page.
// Dynamic values refresh in small rectangles; NO fillScreen() on any timer.
// InfoTAB v0.7: restrained industrial HMI, 4 horizontal swipe pages.
// Only page changes clear the whole LCD. Telemetry changes repaint values.
static constexpr uint16_t UI_BG = 0x322A;    // brighter charcoal
static constexpr uint16_t UI_CARD = 0x532E;  // medium graphite
static constexpr uint16_t UI_BORDER = 0x7453;
static constexpr uint16_t UI_WHITE = 0xFFFF;
static constexpr uint16_t UI_MUTED = 0xD71D;
static constexpr uint16_t UI_ACCENT = 0x8678; // muted teal
static constexpr uint16_t UI_GOOD = 0x9EF6;
static constexpr uint16_t UI_WARNING = 0xFE93;

bool mainPage(Page p) {
    return p == Page::Dashboard || p == Page::Power ||
           p == Page::NetworkInfo || p == Page::Service;
}
int mainPageIndex(Page p) {
    switch (p) {
        case Page::Dashboard: return 0;
        case Page::NetworkInfo: return 1;
        case Page::Power: return 2;
        case Page::Service: return 3;
        default: return -1;
    }
}
Page mainPageByIndex(int idx) {
    static constexpr Page pages[] = {
        Page::Dashboard, Page::NetworkInfo, Page::Power, Page::Service
    };
    return pages[constrain(idx, 0, 3)];
}
void uiText(int x, int y, const String& s, uint16_t color,
            int scale = 2, uint16_t background = UI_CARD) {
    M5.Display.setTextColor(color, background);
    M5.Display.setTextSize(scale);
    M5.Display.setCursor(x, y);
    M5.Display.print(s);
}
void valueAt(int x, int y, int w, int h, const String& s,
             uint16_t color = UI_WHITE, int scale = 3,
             uint16_t background = UI_CARD) {
    M5.Display.fillRect(x, y, w, h, background);
    uiText(x, y + 3, s, color, scale, background);
}
void panel(int x, int y, int w, int h) {
    M5.Display.fillRoundRect(x, y, w, h, 12, UI_CARD);
    M5.Display.drawRoundRect(x, y, w, h, 12, UI_BORDER);
}
void rule(int x, int y, int w) {
    M5.Display.drawFastHLine(x, y, w, UI_BORDER);
}
// No topbar. Content occupies the whole area above bottom navigation.
void tabShell(const char* /*module*/, int active) {
    M5.Display.fillScreen(UI_BG);
    M5.Display.drawFastHLine(32, 624, displayWidth - 64, UI_BORDER);
    static const char* labels[] = {"AI PLATFORM", "NAS", "UPS", "USTAWIENIA"};
    for (int i = 0; i < 4; ++i) {
        const int x = 32 + i * 307;
        const bool selected = active == i;
        if (selected) M5.Display.fillRoundRect(x, 640, 284, 60, 9, UI_CARD);
        uiText(x + 27, 663, labels[i], selected ? UI_WHITE : UI_MUTED,
               2, selected ? UI_CARD : UI_BG);
        if (selected) M5.Display.fillRect(x + 16, 697, 252, 3, UI_ACCENT);
    }
}
String connectionStatus() {
    return connected() ? "POLACZONO" : "BRAK WIFI";
}
String upsState() {
    if (!ups.valid) return "BRAK DANYCH Z UPS";
    if (ups.modeCode == 4) return "ZASILANIE PRAWIDLOWE";
    return String("TRYB UPS: ") + String(ups.modeCode);
}
uint16_t upsStateColor() {
    if (!ups.valid) return UI_WARNING;
    return ups.modeCode == 4 ? UI_GOOD : UI_WARNING;
}
void progressBar(int x, int y, int width, int pct, uint16_t color) {
    M5.Display.fillRoundRect(x, y, width, 10, 4, UI_BORDER);
    if (pct >= 0 && pct <= 100) {
        const int fill = (pct * width) / 100;
        if (fill > 0) M5.Display.fillRoundRect(x, y, fill, 10, 4, color);
    }
}
// AI Platform page: only fields actually reported by GET /health.
// Separate tiles do not imply that a not-checked Ollama component is healthy.
void drawOverviewValues() {
    valueAt(57, 154, 540, 65,
        aiBridgeChecked ? (aiBridgeOk ? "ONLINE" : "OFFLINE") : "SPRAWDZANIE",
        aiBridgeOk ? UI_GOOD : UI_WARNING, 4);
    valueAt(675, 154, 535, 65, aiBridgeOk ? String("v") + aiBridgeVersion : "--",
            UI_WHITE, 3);
    valueAt(57, 440, 540, 58,
        aiBridgeOk ? aiDatabaseState : "--",
        aiDatabaseState == "ok" ? UI_GOOD : UI_WARNING, 3);
    valueAt(675, 440, 535, 58,
        aiBridgeOk ? (aiOllamaState == "not_checked" ?
            "NIE SPRAWDZONO" : aiOllamaState) : "--",
        UI_WHITE, 2);
}
void drawOverview() {
    tabShell("", 0);
    panel(32, 24, 595, 265);
    panel(651, 24, 597, 265);
    panel(32, 308, 595, 266);
    panel(651, 308, 597, 266);
    uiText(57, 53, "AI PLATFORM", UI_WHITE, 3);
    uiText(57, 105, "STAN POLACZENIA", UI_MUTED, 2);
    rule(57, 246, 540);
    uiText(57, 259, "Host: 192.168.1.100", UI_MUTED, 1);

    uiText(675, 53, "AI BRIDGE", UI_WHITE, 3);
    uiText(675, 105, "WERSJA API", UI_MUTED);
    rule(675, 246, 535);
    uiText(675, 259, "GET /health  |  HTTP 200", UI_MUTED, 1);

    uiText(57, 337, "BAZA DANYCH", UI_WHITE, 3);
    uiText(57, 391, "STAN Z API", UI_MUTED);
    rule(57, 529, 540);
    uiText(57, 540, "Wartosc zwracana przez AI Bridge", UI_MUTED, 1);

    uiText(675, 337, "OLLAMA", UI_WHITE, 3);
    uiText(675, 391, "KONTROLA KOMPONENTU", UI_MUTED);
    rule(675, 529, 535);
    uiText(675, 540, "Brak kontroli nie oznacza awarii", UI_MUTED, 1);

    uiText(42, 590, "DANE: 192.168.1.100:8080/health", UI_MUTED, 2, UI_BG);
    drawOverviewValues();
}
// Eight aligned tiles; each live measurement has its own light graphite card.
// UPS: the entire module is eight tiles. No topbar or outside diagnostics.
void powerTile(int col, int row, const String& label, const String& note) {
    const int x = 32 + col * 308, y = 24 + row * 287;
    panel(x, y, 292, 264);
    uiText(x + 21, y + 30, label, UI_MUTED, 2);
    rule(x + 21, y + 213, 249);
    uiText(x + 21, y + 227, note, UI_MUTED, 1);
}
void tilePowerValue(int col, int row, const String& value,
                    uint16_t color = UI_WHITE, int scale = 3) {
    const int x = 53 + col * 308, y = 140 + row * 287;
    valueAt(x, y, 249, 70, value, color, scale);
}
void drawPowerValues() {
    tilePowerValue(0, 0, ups.valid ? String(ups.inputVoltTenths / 10.0f, 1) + " V" : "--");
    tilePowerValue(1, 0, ups.valid ? String(ups.outputVoltTenths / 10.0f, 1) + " V" : "--");
    tilePowerValue(2, 0, ups.valid ? String(ups.outputHzTenths / 10.0f, 1) + " Hz" : "--");
    tilePowerValue(3, 0, ups.valid && ups.powerWattsEstimate >= 0 ?
                   String("~") + String(ups.powerWattsEstimate) + " W" : "--");
    tilePowerValue(0, 1, ups.valid ? String(ups.batteryPct) + "%" : "--",
                   ups.valid ? UI_GOOD : UI_WARNING, 4);
    tilePowerValue(1, 1, ups.valid ? String(ups.loadPct) + "%" : "--", UI_WHITE, 4);
    tilePowerValue(2, 1, ups.valid ? String(ups.runtimeSeconds / 60) + " min" : "--");
    tilePowerValue(3, 1, ups.valid && ups.batteryVoltTenths > 0 ?
                   String(ups.batteryVoltTenths / 10.0f, 1) + " V" : "--");
}
void drawPower() {
    tabShell("", 2);
    powerTile(0, 0, "WEJSCIE", "Napiecie sieci");
    powerTile(1, 0, "WYJSCIE", "Napiecie UPS");
    powerTile(2, 0, "CZESTOTLIWOSC", "Wyjscie UPS");
    powerTile(3, 0, "MOC WYJ. *", "Szacunek / reg156");
    powerTile(0, 1, "BATERIA", "Naladowanie");
    powerTile(1, 1, "OBCIAZENIE", "Procent nominalny");
    powerTile(2, 1, "CZAS UPS", "Szacunek urzadzenia");
    powerTile(3, 1, "NAP. BATERII", "Akumulator");
    drawPowerValues();
}
// NAS screen uses verified TCP port reachability only.
// Disk/RAID/backup metrics require a future authenticated QNAP API.
void drawNetworkValues() {
    const bool online = nasChecked && (nasHttps || nasQts);
    valueAt(58, 145, 540, 64,
            !nasChecked ? "SPRAWDZANIE" : (online ? "DOSTEPNY" : "NIEDOSTEPNY"),
            online ? UI_GOOD : UI_WARNING, 4);
    valueAt(675, 145, 535, 64,
            !nasChecked ? "--" : (nasQts ? "TCP 8080  OK" : "BRAK POLACZENIA"),
            nasQts ? UI_GOOD : UI_WARNING, 3);
    valueAt(58, 429, 540, 64,
            !nasChecked ? "--" : (nasHttps ? "TCP 443  OK" : "BRAK POLACZENIA"),
            nasHttps ? UI_GOOD : UI_WARNING, 3);
    valueAt(675, 429, 535, 64, "BRAK DANYCH API", UI_MUTED, 3);
}
void drawNetwork() {
    tabShell("", 1);
    panel(32, 24, 595, 264);
    panel(651, 24, 597, 264);
    panel(32, 308, 595, 264);
    panel(651, 308, 597, 264);
    uiText(58, 54, "GLOBAL NAS", UI_WHITE, 3);
    uiText(58, 100, "DOSTEPNOSC SIECIOWA", UI_MUTED);
    rule(58, 246, 541);
    uiText(58, 260, "IP: 192.168.1.101", UI_MUTED, 1);

    uiText(675, 54, "PANEL QNAP", UI_WHITE, 3);
    uiText(675, 100, "POLACZENIE TCP", UI_MUTED);
    rule(675, 246, 535);
    uiText(675, 260, "Port 8080  /  bez logowania", UI_MUTED, 1);

    uiText(58, 338, "HTTPS", UI_WHITE, 3);
    uiText(58, 386, "DOSTEPNOSC PORTU", UI_MUTED);
    rule(58, 529, 541);
    uiText(58, 541, "Port 443  /  bez pobierania danych", UI_MUTED, 1);

    uiText(675, 338, "DYSKI / BACKUP", UI_WHITE, 3);
    uiText(675, 386, "DANE WYMAGAJA QNAP API", UI_MUTED);
    rule(675, 529, 535);
    uiText(675, 541, "Nie pokazujemy nieznanych stanow", UI_MUTED, 1);

    uiText(42, 590, "MONITORING SIECI NAS  /  brak danych o RAID i backupie",
           UI_MUTED, 2, UI_BG);
    drawNetworkValues();
}
// All configuration and maintenance actions are on this page.
void drawServiceValues() {
    valueAt(57, 136, 540, 47,
            connected() ? String("ONLINE  /  ") + WiFi.localIP().toString() :
            String("WIFI NIEDOSTEPNE"), connected() ? UI_GOOD : UI_WARNING, 2);
    valueAt(675, 136, 535, 47,
            otaArmed ? "OTA AKTYWNE" : "OTA WYLACZONE",
            otaArmed ? UI_GOOD : UI_WHITE, 2);
    valueAt(57, 427, 540, 44, "InfoTAB v0.9", UI_WHITE, 3);
    valueAt(57, 509, 540, 39,
            String("Czas pracy: ") + String(millis() / 60000) + " min", UI_MUTED, 2);
}
void drawService() {
    tabShell("", 3);
    panel(32, 24, 595, 265);
    panel(651, 24, 597, 265);
    panel(32, 308, 595, 266);
    panel(651, 308, 597, 266);

    uiText(57, 55, "WI-FI", UI_WHITE, 3);
    uiText(57, 105, "POLACZENIE INFOTAB", UI_MUTED);
    M5.Display.fillRoundRect(57, 207, 540, 63, 9, UI_BORDER);
    uiText(81, 228, "KONFIGURUJ SIEC", UI_WHITE, 2, UI_BORDER);

    uiText(675, 55, "OTA / WIFI", UI_WHITE, 3);
    uiText(675, 105, "AKTUALIZACJA FIRMWARE", UI_MUTED);
    M5.Display.fillRoundRect(675, 207, 540, 63, 9, UI_BORDER);
    uiText(699, 228, "AKTUALIZUJ", UI_WHITE, 2, UI_BORDER);

    uiText(57, 337, "OPROGRAMOWANIE", UI_WHITE, 3);
    uiText(57, 390, "WERSJA SYSTEMU", UI_MUTED);

    uiText(675, 337, "DIAGNOSTYKA", UI_WHITE, 3);
    uiText(675, 390, "PARTYCJE / AI BRIDGE", UI_MUTED);
    M5.Display.fillRoundRect(675, 489, 540, 64, 9, UI_BORDER);
    uiText(699, 511, "OTWORZ DIAGNOSTYKE", UI_WHITE, 2, UI_BORDER);
    drawServiceValues();
}

void drawDashboardValues() {
    switch(page) {
        case Page::Dashboard: drawOverviewValues(); break;
        case Page::Power: drawPowerValues(); break;
        case Page::NetworkInfo: drawNetworkValues(); break;
        case Page::Service: drawServiceValues(); break;
        default: break;
    }
}
void drawDashboard() { drawOverview(); }
void refreshDashboard() {
    if (page == Page::Dashboard) pollAiBridge();
    else if (page == Page::NetworkInfo) pollNas();
    else if (page == Page::Power) pollUpsReadOnly();
    lastDashboardRefresh = millis();
    if (mainPage(page)) drawDashboardValues();
}
void openMainPage(Page target) {
    if (!mainPage(target) || page == target) return;
    page = target;
    render();
    lastDashboardRefresh = 0; // fetch fresh data for newly selected module
}
bool handleNavigationTap(int x, int y) {
    if (!mainPage(page) || y < 632 || y > 708) return false;
    for (int i = 0; i < 4; ++i) {
        const int left = 32 + i * 307;
        if (inside(x, y, left, 632, 285, 76)) {
            openMainPage(mainPageByIndex(i));
            return true;
        }
    }
    return false;
}
void handleDashboard(int x, int y) {
    if (handleNavigationTap(x,y)) return;
    // Overview is read-only; use horizontal swipe or bottom tabs to navigate.
}
void handlePower(int x, int y) {
    handleNavigationTap(x,y);
}
void handleNetworkInfo(int x, int y) {
    handleNavigationTap(x,y);
}
void handleService(int x, int y) {
    if (handleNavigationTap(x,y)) return;
    if (inside(x,y,57,207,540,63)) {
        page=Page::Home;
        render();
    } else if (inside(x,y,675,207,540,63)) {
        page=Page::OtaUpload;
        render();
    } else if (inside(x,y,675,489,540,64)) {
        page=Page::OtaDiag;
        infoMessage="Diagnostyka odczytu; bez zmiany Flash.";
        render();
    }
}

void drawHome() {
    header("Siec / ustawienia");
    const bool online = connected();
    writeText(45, 124, online ? "POLACZONO Z WI-FI" : "WI-FI: BRAK POLACZENIA",
              online ? TFT_GREEN : TFT_YELLOW, 3);
    if (online) {
        writeText(45, 190, "SSID: " + WiFi.SSID(), TFT_WHITE, 2);
        writeText(45, 235, "Adres IP: " + WiFi.localIP().toString(), TFT_CYAN, 3);
        writeText(45, 292, "RSSI: " + String(WiFi.RSSI()) + " dBm", TFT_WHITE, 2);
    } else if (savedSsid.length()) {
        writeText(45, 190, "Zapisana siec: " + savedSsid, TFT_WHITE);
        writeText(45, 235, "Mozesz sprobowac ponownie.", TFT_LIGHTGREY);
    } else {
        writeText(45, 190, "Nie skonfigurowano sieci.", TFT_WHITE);
        writeText(45, 235, "Wybierz siec i wpisz haslo na ekranie.", TFT_LIGHTGREY);
    }
    if (infoMessage.length()) {
        writeText(45, 355, infoMessage.substring(0, 62), TFT_YELLOW);
    }
    button(45, 450, 330, 90, "WYBIERZ SIEC", TFT_DARKCYAN, 2);
    if (savedSsid.length()) {
        button(408, 450, 335, 90, "POLACZ PONOWNIE", TFT_DARKGREEN, 2);
        button(777, 450, 335, 90, "ZAPOMNIJ SIEC", TFT_MAROON, 2);
    }
    button(45, 557, 375, 67, "OTA DIAG (ODCZYT)", TFT_NAVY, 2);
    button(450, 557, 330, 67, "DO USTAWIEN", TFT_DARKGREEN, 2);
    writeText(45, displayHeight - 62, "Konfiguracja lokalna | bez hasel w GitHub", TFT_LIGHTGREY);
}

void drawNetworks() {
    header("Wybierz siec Wi-Fi");
    writeText(40, 108, "Dotknij nazwy sieci. Pokazano do 32 wynikow.", TFT_LIGHTGREY);
    for (int i = 0; i < NETWORKS_PER_PAGE; ++i) {
        const int index = networkPage * NETWORKS_PER_PAGE + i;
        if (index >= networkCount) break;
        const int y = 155 + i * 64;
        M5.Display.fillRoundRect(38, y, displayWidth - 76, 57, 8, TFT_DARKGREY);
        M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
        M5.Display.setTextSize(2);
        M5.Display.setCursor(56, y + 16);
        String name = networks[index].ssid;
        if (name.length() > 34) name = name.substring(0, 34) + "...";
        M5.Display.print(name);
        M5.Display.setCursor(displayWidth - 345, y + 16);
        M5.Display.print(String(networks[index].rssi) + " dBm");
        M5.Display.setCursor(displayWidth - 158, y + 16);
        M5.Display.print(networks[index].secured ? "LOCK" : "OPEN");
    }
    button(38, 636, 200, 62, "WROC", TFT_DARKGREY);
    button(265, 636, 246, 62, "ODSWIEZ");
    if (networkPage > 0) button(540, 636, 185, 62, "POPRZ.");
    if ((networkPage + 1) * NETWORKS_PER_PAGE < networkCount)
        button(750, 636, 185, 62, "DALEJ");
    writeText(displayWidth - 300, 653, "Znaleziono: " + String(networkCount), TFT_LIGHTGREY);
    if (networkCount == 0) writeText(50, 260, infoMessage, TFT_YELLOW, 2);
}

const char* keyRows(bool special, int row) {
    static const char* letters[] = {
        "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"
    };
    static const char* symbols[] = {
        "1234567890", "!@#$%^&*()", "-_=+[]{};:", "'\",.<>/?\\|"
    };
    return (special ? symbols : letters)[row];
}

char keyChar(const char* row, int index) {
    char value = row[index];
    if (upperCase && !symbolMode && value >= 'a' && value <= 'z')
        value = static_cast<char>(value - ('a' - 'A'));
    return value;
}

void drawPassword() {
    header("Haslo sieci Wi-Fi");
    button(displayWidth - 180, 18, 140, 56, "WROC", TFT_DARKGREY);
    writeText(43, 110, "SIEC: " + pendingSsid.substring(0, 44), TFT_CYAN, 2);
    writeText(43, 156, "Wpisz haslo (8-63 znaki ASCII):", TFT_WHITE);
    M5.Display.drawRoundRect(40, 205, displayWidth - 80, 73, 8, TFT_LIGHTGREY);
    String dots;
    for (size_t i = 0; i < pendingPassword.length(); ++i) dots += '*';
    writeText(60, 224, dots, TFT_GREEN, 2);
    writeText(45, 286, infoMessage, TFT_YELLOW, 2);

    const int keyWidth = (displayWidth - 2 * MARGIN - 9 * KEY_GAP) / 10;
    for (int r = 0; r < 4; ++r) {
        const char* keys = keyRows(symbolMode, r);
        const int len = strlen(keys);
        const int x0 = (displayWidth - (len * keyWidth + (len - 1) * KEY_GAP)) / 2;
        for (int i = 0; i < len; ++i) {
            const char c = keyChar(keys, i);
            button(x0 + i * (keyWidth + KEY_GAP), KEY_FIRST_Y + r * 63,
                   keyWidth, KEY_HEIGHT, String(c), TFT_DARKGREY, 2);
        }
    }
    const int y = 580;
    button(32, y, 170, 67, upperCase ? "shift ON" : "SHIFT", TFT_NAVY);
    button(216, y, 145, 67, symbolMode ? "ABC" : "SYM", TFT_NAVY);
    button(376, y, 340, 67, "SPACJA", TFT_DARKGREY);
    button(733, y, 180, 67, "KASUJ", TFT_MAROON);
    button(931, y, displayWidth - 963, 67, "POLACZ", TFT_DARKGREEN);
    writeText(43, 680, "Haslo pozostaje w pamieci urzadzenia (NVS).", TFT_LIGHTGREY);
}


// Diagnostic-only: these APIs inspect partition metadata; no flash writes.
void drawOtaDiag() {
    header("Diagnostyka OTA (TYLKO ODCZYT)");
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    const uint32_t sketchBytes = ESP.getSketchSize();
    writeText(45, 116, "Firmware InfoTAB: v0.8", TFT_CYAN, 2);
    writeText(45, 160, "Obraz programu: " + String(sketchBytes) + " B", TFT_WHITE, 2);
    if (running) {
        writeText(45, 218, "Aktywna: " + String(running->label), TFT_GREEN, 2);
        writeText(45, 257, "Offset: 0x" + String((unsigned long)running->address, HEX) +
                  "    Rozmiar: " + String(running->size / 1024) + " KiB", TFT_WHITE, 2);
    } else {
        writeText(45, 218, "Brak aktywnej partycji!", TFT_RED, 2);
    }
    if (next) {
        writeText(45, 325, "Nastepny slot OTA: " + String(next->label), TFT_CYAN, 2);
        writeText(45, 364, "Offset: 0x" + String((unsigned long)next->address, HEX) +
                  "    Rozmiar: " + String(next->size / 1024) + " KiB", TFT_WHITE, 2);
    } else {
        writeText(45, 325, "Brak slotu OTA: aktualizacja ZABLOKOWANA", TFT_RED, 2);
    }
    writeText(45, 423, boot && running && boot == running ?
              "Boot slot zgodny z uruchomionym" : "Boot slot wymaga weryfikacji",
              boot && running && boot == running ? TFT_GREEN : TFT_YELLOW, 2);
    writeText(45, 472, "Rollback: jeszcze nie przetestowano", TFT_YELLOW, 2);
    writeText(45, 520, connected() ? "Wi-Fi: polaczono" : "Wi-Fi: offline",
              connected() ? TFT_GREEN : TFT_YELLOW, 2);
    button(45, 611, 270, 72, "WROC", TFT_DARKGREY, 2);
    button(348, 611, 420, 72, "TEST AI BRIDGE", TFT_DARKCYAN, 2);
    writeText(45, 568, infoMessage.substring(0, 80), TFT_LIGHTGREY, 2);

    // USB log gives evidence without revealing credentials or SSID.
    Serial.printf("[InfoTAB] OTA_DIAG sketch=%lu running=%s run_size=%lu next=%s next_size=%lu boot=%s\n",
                  static_cast<unsigned long>(sketchBytes),
                  running ? running->label : "NONE",
                  running ? static_cast<unsigned long>(running->size) : 0UL,
                  next ? next->label : "NONE",
                  next ? static_cast<unsigned long>(next->size) : 0UL,
                  boot ? boot->label : "NONE");
}

void checkBridgeHealth() {
    if (!connected()) {
        infoMessage = "Wi-Fi offline. Wroc do ustawien sieci.";
        drawOtaDiag();
        return;
    }
    NetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(1500);
    http.setTimeout(2000);
    int status = -1;
    // Arduino ESP32 3.2.1: prefer the unconditional begin(NetworkClient&, String)
    // rather than the optional legacy begin(String) overload.
    if (http.begin(client, String(BRIDGE_HEALTH))) {
        status = http.GET();
        http.end();
    }
    // HTTP 200 only checks reachability, not OTA readiness/authentication.
    infoMessage = status == 200 ? "AI BRIDGE: HTTP 200 (ONLINE)" :
                  "AI BRIDGE: brak HTTP 200, kod " + String(status);
    Serial.printf("[InfoTAB] BRIDGE_HEALTH HTTP=%d\n", status);
    drawOtaDiag();
}


void drawOtaUpload() {
    header("AKTUALIZACJA FIRMWARE PRZEZ WI-FI");
    writeText(48, 123, "OTA / TRYB SERWISOWY", TFT_CYAN, 3);
    writeText(48, 185, "IP InfoTAB: " +
              (connected() ? WiFi.localIP().toString() : String("BRAK POLACZENIA")),
              connected() ? TFT_GREEN : TFT_YELLOW, 3);
    writeText(48, 237, "Port OTA: 3232   |   Slot A/B: 1280 KiB", TFT_WHITE, 2);

    M5.Display.drawRoundRect(43, 302, displayWidth - 86, 201, 12, TFT_DARKGREY);
    if (otaArmed) {
        writeText(65, 338, otaInProgress ? "PRZESYLANIE FIRMWARE" :
                  "OTA AKTYWNE - ODBIERA Z WINDOWS", TFT_GREEN, 3);
        writeText(65, 414, "Sesja serwisowa: maks. 2 minuty.", TFT_LIGHTGREY, 2);
        writeText(65, 460, "Bez hasla - tylko zaufana siec LAN.", TFT_YELLOW, 2);
    } else {
        writeText(65, 348, "Odbior OTA WYLACZONY.", TFT_LIGHTGREY, 3);
        writeText(65, 419, "Aktywuj tylko przed aktualizacja.", TFT_YELLOW, 2);
    }
    M5.Display.fillRect(48, 518, displayWidth - 96, 55, 0x322A);
    writeText(48, 525, otaStatus.substring(0, 75), TFT_CYAN, 2);
    button(46, 612, 263, 78, "WROC", TFT_DARKGREY, 2);
    if (!otaInProgress) {
        button(331, 612, 403, 78, otaArmed ? "ZATRZYMAJ OTA" : "AKTYWUJ OTA",
               otaArmed ? TFT_MAROON : TFT_DARKGREEN, 2);
    }
    writeText(785, 631, "Nie zmienia partycji Flash", TFT_LIGHTGREY, 2);
}

void stopOta() {
    if (otaInProgress) return;
    if (otaArmed) ArduinoOTA.end();
    otaArmed = false;
}

void armOta() {
    if (otaInProgress) return;
    if (!connected()) {
        otaStatus = "Brak Wi-Fi. Polacz z siecia.";
        render();
        return;
    }
    const esp_partition_t *slot = esp_ota_get_next_update_partition(nullptr);
    if (!slot || slot->size < ESP.getSketchSize()) {
        otaStatus = "Brak poprawnej partycji OTA.";
        render();
        return;
    }
    // Manual on-device arming; 120s only. Warning: during this period
    // other LAN clients could attempt OTA; use trusted/isolated LAN.
    ArduinoOTA.setHostname("infotab");
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setMdnsEnabled(false);
    ArduinoOTA.onStart([]() {
        otaInProgress = true;
        otaStatus = "Przesylanie firmware...";
        if (page == Page::OtaUpload) drawOtaUpload();
        logEvent("OTA transfer started");
    });
    ArduinoOTA.onEnd([]() {
        otaStatus = "Wgrywanie zakonczone. Uruchamianie...";
        logEvent("OTA transfer completed");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        otaInProgress = false;
        otaStatus = "Blad aktualizacji OTA: " + String(static_cast<int>(error));
        if (page == Page::OtaUpload) drawOtaUpload();
        logEvent("OTA transfer error");
    });
    ArduinoOTA.begin();
    otaArmedAt = millis();
    otaArmed = true;
    otaStatus = "Uruchom uploader bez hasla na Windows.";
    render();
    logEvent("OTA armed without password (120s service window)");
}

void handleOtaUpload(int x, int y) {
    if (otaInProgress) return;
    if (inside(x, y, 46, 612, 263, 78)) {
        stopOta();
        page = Page::Service;
        render();
    } else if (inside(x, y, 331, 612, 403, 78)) {
        if (otaArmed) {
            stopOta();
            otaStatus = "OTA zostalo wylaczone.";
            render();
        } else armOta();
    }
}

void drawConnecting() {
    header("Laczenie z Wi-Fi");
    writeText(55, 160, "Proba polaczenia...", TFT_CYAN, 3);
    writeText(55, 245, "SIEC: " + pendingSsid.substring(0, 44), TFT_WHITE);
    writeText(55, 300, "Maksymalnie 20 sekund", TFT_LIGHTGREY);
    writeText(55, 365, "Nie odlaczaj zasilania.", TFT_YELLOW);
    button(55, 485, 285, 70, "ANULUJ", TFT_MAROON);
}

void render() {
    switch (page) {
        case Page::Dashboard: drawDashboard(); break;
        case Page::Power: drawPower(); break;
        case Page::NetworkInfo: drawNetwork(); break;
        case Page::Service: drawService(); break;
        case Page::Home: drawHome(); break;
        case Page::Networks: drawNetworks(); break;
        case Page::Password: drawPassword(); break;
        case Page::Connecting: drawConnecting(); break;
        case Page::OtaDiag: drawOtaDiag(); break;
        case Page::OtaUpload: drawOtaUpload(); break;
    }
}

bool initWifi() {
    if (wifiReady) return true;
    WiFi.setPins(SDIO_CLK, SDIO_CMD, SDIO_D0, SDIO_D1,
                 SDIO_D2, SDIO_D3, SDIO_RST);
    if (!WiFi.mode(WIFI_STA)) {
        logEvent("ERROR: Wi-Fi SDIO init failed");
        return false;
    }
    wifiReady = true;
    return true;
}

void readSaved() {
    Preferences prefs;
    if (!prefs.begin("infotab_wifi", true)) return;
    savedSsid = prefs.getString("ssid", "");
    savedPassword = prefs.getString("password", "");
    prefs.end();
}

bool saveCredentials() {
    Preferences prefs;
    if (!prefs.begin("infotab_wifi", false)) return false;
    // Save only after a successful connection. No serial logging of secrets.
    const size_t n = prefs.putString("ssid", pendingSsid);
    prefs.putString("password", pendingPassword);
    prefs.end();
    return n > 0;
}

void forgetSaved() {
    if (wifiReady) WiFi.disconnect();
    Preferences prefs;
    if (prefs.begin("infotab_wifi", false)) {
        prefs.remove("ssid");
        prefs.remove("password");
        prefs.end();
    }
    savedSsid = "";
    savedPassword = "";
    pendingSsid = "";
    pendingPassword = "";
    infoMessage = "Usunieto zapisana siec.";
    page = Page::Home;
    render();
    logEvent("Saved Wi-Fi settings cleared");
}

void startConnection(bool shouldSave) {
    if (!initWifi()) {
        infoMessage = "Blad inicjalizacji Wi-Fi";
        page = Page::Home;
        render();
        return;
    }
    WiFi.disconnect();
    saveAfterSuccess = shouldSave;
    connectionStart = millis();
    page = Page::Connecting;
    render();
    if (pendingPassword.isEmpty())
        WiFi.begin(pendingSsid.c_str());
    else
        WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());
    logEvent("Wi-Fi connection started (credentials hidden)");
}

void scanNetworks() {
    page = Page::Networks;
    infoMessage = "Skanowanie...";
    networkCount = 0;
    networkPage = 0;
    render();

    if (!initWifi()) {
        infoMessage = "Nie mozna uruchomic ESP32-C6.";
        render();
        return;
    }
    const int total = WiFi.scanNetworks();
    if (total < 0) {
        infoMessage = "Nie udalo sie przeskanowac sieci.";
    } else {
        for (int i = 0; i < total && networkCount < MAX_NETWORKS; ++i) {
            const String ssid = WiFi.SSID(i);
            if (ssid.isEmpty()) continue; // Hidden SSIDs not configured in v0.3.
            networks[networkCount].ssid = ssid;
            networks[networkCount].rssi = WiFi.RSSI(i);
            networks[networkCount].secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            ++networkCount;
        }
        infoMessage = networkCount ? "" : "Nie znaleziono widocznych sieci.";
    }
    WiFi.scanDelete();
    render();
    Serial.printf("[InfoTAB] Scan completed; visible networks=%d\n", networkCount);
}

void selectNetwork(int index) {
    pendingSsid = networks[index].ssid;
    passwordSecure = networks[index].secured;
    pendingPassword = "";
    upperCase = false;
    symbolMode = false;
    infoMessage = "";
    if (!passwordSecure) {
        startConnection(true);
    } else {
        page = Page::Password;
        render();
    }
}

void handleHome(int x, int y) {
    if (inside(x, y, 450, 557, 330, 67)) {
        page = Page::Service;
        render();
    } else if (inside(x, y, 45, 557, 375, 67)) {
        page = Page::OtaDiag;
        infoMessage = "Nacisnij TEST AI BRIDGE, by sprawdzic API.";
        render();
    } else if (inside(x, y, 45, 450, 330, 90)) {
        scanNetworks();
    } else if (savedSsid.length() && inside(x, y, 408, 450, 335, 90)) {
        pendingSsid = savedSsid;
        pendingPassword = savedPassword;
        startConnection(false);
    } else if (savedSsid.length() && inside(x, y, 777, 450, 335, 90)) {
        forgetSaved();
    }
}

void handleOtaDiag(int x, int y) {
    if (inside(x, y, 45, 611, 270, 72)) {
        page = Page::Service;
        infoMessage = "";
        render();
    } else if (inside(x, y, 348, 611, 420, 72)) {
        infoMessage = "Trwa test API...";
        drawOtaDiag();
        checkBridgeHealth();
    }
}

void handleNetworks(int x, int y) {
    for (int i = 0; i < NETWORKS_PER_PAGE; ++i) {
        const int index = networkPage * NETWORKS_PER_PAGE + i;
        if (index < networkCount && inside(x, y, 38, 155 + i * 64,
                                            displayWidth - 76, 57)) {
            selectNetwork(index);
            return;
        }
    }
    if (inside(x, y, 38, 636, 200, 62)) {
        page = Page::Home;
        render();
    } else if (inside(x, y, 265, 636, 246, 62)) {
        scanNetworks();
    } else if (networkPage > 0 && inside(x, y, 540, 636, 185, 62)) {
        --networkPage;
        render();
    } else if ((networkPage + 1) * NETWORKS_PER_PAGE < networkCount &&
               inside(x, y, 750, 636, 185, 62)) {
        ++networkPage;
        render();
    }
}

void handlePassword(int x, int y) {
    if (inside(x, y, displayWidth - 180, 18, 140, 56)) {
        pendingPassword = "";
        page = Page::Networks;
        render();
        return;
    }
    const int keyWidth = (displayWidth - 2 * MARGIN - 9 * KEY_GAP) / 10;
    for (int r = 0; r < 4; ++r) {
        const char* row = keyRows(symbolMode, r);
        const int len = strlen(row);
        const int x0 = (displayWidth - (len * keyWidth + (len - 1) * KEY_GAP)) / 2;
        for (int i = 0; i < len; ++i) {
            if (inside(x, y, x0 + i * (keyWidth + KEY_GAP),
                       KEY_FIRST_Y + r * 63, keyWidth, KEY_HEIGHT)) {
                if (pendingPassword.length() < 63)
                    pendingPassword += keyChar(row, i);
                infoMessage = "";
                render();
                return;
            }
        }
    }
    if (inside(x, y, 32, 580, 170, 67)) {
        upperCase = !upperCase;
    } else if (inside(x, y, 216, 580, 145, 67)) {
        symbolMode = !symbolMode;
    } else if (inside(x, y, 376, 580, 340, 67)) {
        if (pendingPassword.length() < 63) pendingPassword += ' ';
    } else if (inside(x, y, 733, 580, 180, 67)) {
        if (pendingPassword.length()) pendingPassword.remove(pendingPassword.length() - 1);
    } else if (inside(x, y, 931, 580, displayWidth - 963, 67)) {
        if (pendingPassword.length() < 8) {
            infoMessage = "Haslo musi miec co najmniej 8 znakow.";
        } else {
            startConnection(true);
            return;
        }
    } else {
        return;
    }
    render();
}

void handleConnecting(int x, int y) {
    if (inside(x, y, 55, 485, 285, 70)) {
        WiFi.disconnect();
        pendingPassword = "";
        infoMessage = "Polaczenie anulowane.";
        page = Page::Home;
        render();
    }
}

void updateConnection() {
    if (page != Page::Connecting) return;
    if (connected()) {
        bool stored = true;
        if (saveAfterSuccess) {
            stored = saveCredentials();
            savedSsid = pendingSsid;
            savedPassword = pendingPassword;
        }
        pendingPassword = "";
        infoMessage = stored ? "Polaczenie udane." :
                              "Polaczono, ale zapis ustawien nie powiodl sie.";
        page = Page::Dashboard;
        render();
        refreshDashboard();
        logEvent("Wi-Fi connected; IP available on dashboard");
    } else if (millis() - connectionStart > CONNECT_TIMEOUT_MS) {
        WiFi.disconnect();
        pendingPassword = "";
        infoMessage = "Blad polaczenia / timeout. Sprobuj ponownie.";
        page = Page::Home;
        render();
        logEvent("Wi-Fi connection timeout");
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    if (M5.Display.width() < M5.Display.height())
        M5.Display.setRotation(0);
    displayWidth = M5.Display.width();
    displayHeight = M5.Display.height();
    logEvent("v0.9 boot: AI Platform/NAS/UPS/Settings modular HMI");
    readSaved();
    infoMessage = "Wybierz siec na ekranie.";
    render();
    if (savedSsid.length()) {
        pendingSsid = savedSsid;
        pendingPassword = savedPassword;
        startConnection(false);
    }
}

void loop() {
    M5.update();
    const auto touch = M5.Touch.getDetail();
    // Gesture recognition: only 4 main pages accept horizontal swipes.
    // A press/release with <90 px travel remains a normal button tap.
    static bool trackingTouch = false;
    static int startX = 0, startY = 0, lastX = 0, lastY = 0;
    static uint32_t touchStartedAt = 0;
    if (touch.wasPressed() && mainPage(page)) {
        trackingTouch = true;
        startX = lastX = touch.x;
        startY = lastY = touch.y;
        touchStartedAt = millis();
    }
    if (trackingTouch && touch.isPressed()) {
        lastX = touch.x;
        lastY = touch.y;
    }

    bool handledSwipe = false;
    if (trackingTouch && !touch.isPressed()) {
        trackingTouch = false;
        const int deltaX = lastX - startX;
        const int deltaY = lastY - startY;
        if (mainPage(page) && abs(deltaX) >= 90 &&
            abs(deltaX) > 2 * abs(deltaY) &&
            millis() - touchStartedAt < 1500) {
            const int next = mainPageIndex(page) + (deltaX < 0 ? 1 : -1);
            if (next >= 0 && next <= 3) {
                openMainPage(mainPageByIndex(next));
                handledSwipe = true;
            }
        }
    }
    // Tap is dispatched only if no swipe was recognized.
    if (!handledSwipe && touch.wasClicked()) {
        const int x = touch.x;
        const int y = touch.y;
        switch (page) {
            case Page::Dashboard: handleDashboard(x, y); break;
            case Page::Power: handlePower(x, y); break;
            case Page::NetworkInfo: handleNetworkInfo(x, y); break;
            case Page::Service: handleService(x, y); break;
            case Page::Home: handleHome(x, y); break;
            case Page::Networks: handleNetworks(x, y); break;
            case Page::Password: handlePassword(x, y); break;
            case Page::Connecting: handleConnecting(x, y); break;
            case Page::OtaDiag: handleOtaDiag(x, y); break;
            case Page::OtaUpload: handleOtaUpload(x, y); break;
        }
    }

    updateConnection();
    if (otaArmed) {
        if (!otaInProgress &&
            (millis() - otaArmedAt >= OTA_WINDOW_MS || !connected())) {
            stopOta();
            otaStatus = "Sesja OTA wygasla lub utracono Wi-Fi.";
            if (page == Page::OtaUpload) render();
        } else if (connected()) {
            ArduinoOTA.handle();
        }
    }

    // No full-screen redraw on refresh. Only the visible data fields change.
    if (mainPage(page) &&
        (lastDashboardRefresh == 0 ||
         millis() - lastDashboardRefresh >= DASH_REFRESH_MS)) {
        refreshDashboard();
    }
    delay(8);
}
