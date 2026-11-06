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

enum class Page { Dashboard, Home, Networks, Password, Connecting, OtaDiag, OtaUpload };
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

struct UpsData {
    bool valid = false;
    int modeCode = -1;
    int batteryPct = -1;
    int loadPct = -1;
    int inputVoltTenths = -1;
    int outputVoltTenths = -1;
    int outputHzTenths = -1;
    long runtimeSeconds = -1;
    uint32_t updatedAt = 0;
};
UpsData ups;
bool otaArmed = false;
bool otaInProgress = false;
uint32_t otaArmedAt = 0;
constexpr uint32_t OTA_WINDOW_MS = 300000;
String otaPassword;
String otaStatus = "Gotowe do aktywacji aktualizacji.";

void logEvent(const char* event) {
    // Deliberately never log SSIDs or passwords.
    Serial.printf("[InfoTAB] %s\n", event);
}

bool inside(int x, int y, int bx, int by, int w, int h) {
    return x >= bx && x < bx + w && y >= by && y < by + h;
}

void writeText(int x, int y, const String& value, uint16_t color, int scale = 2) {
    M5.Display.setTextColor(color, TFT_BLACK);
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
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.fillRect(0, 0, displayWidth, 90, TFT_NAVY);
    M5.Display.setTextColor(TFT_CYAN, TFT_NAVY);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(MARGIN, 22);
    M5.Display.print("InfoTAB");
    M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
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
    snapshot.runtimeSeconds = m.runtimeSeconds;
    snapshot.updatedAt = millis();
    ups = snapshot;
    Serial.printf("[InfoTAB] Modbus UPS mode=%d battery=%d%% load=%d%% voltage=%d/10V runtime=%lds\n",
                  ups.modeCode, ups.batteryPct, ups.loadPct,
                  ups.outputVoltTenths, ups.runtimeSeconds);
}

void pollAiBridge() {
    aiBridgeChecked = true;
    aiBridgeOk = false;
    if (!connected()) return;
    NetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(1000);
    http.setTimeout(1400);
    if (http.begin(client, String(BRIDGE_HEALTH))) {
        aiBridgeOk = http.GET() == 200;
        http.end();
    }
}

// Static screen elements are drawn only when navigating to this page.
// Dynamic values refresh in small rectangles; NO fillScreen() on any timer.
void drawDashboardValues() {
    const bool good = ups.valid;
    M5.Display.fillRect(52, 183, 742, 48, TFT_BLACK);
    uint16_t statusColor = good && ups.modeCode == 4 ? TFT_GREEN : TFT_YELLOW;
    String state = !good ? "MODBUS: BRAK DANYCH" :
                   ups.modeCode == 4 ? "UPS: TRYB NORMALNY" :
                   "UPS: TRYB " + String(ups.modeCode) + " (sprawdz)";
    writeText(53, 188, state, statusColor, 3);

    M5.Display.fillRect(53, 312, 206, 67, TFT_BLACK);
    M5.Display.fillRect(314, 312, 205, 67, TFT_BLACK);
    M5.Display.fillRect(565, 312, 240, 67, TFT_BLACK);
    writeText(53, 320, good ? String(ups.batteryPct) + "%" : "--",
              good ? TFT_GREEN : TFT_YELLOW, 4);
    writeText(314, 320, good ? String(ups.loadPct) + "%" : "--",
              good ? TFT_WHITE : TFT_YELLOW, 4);
    writeText(565, 320, good ? String(ups.runtimeSeconds / 60) + " min" : "--",
              good ? TFT_CYAN : TFT_YELLOW, 4);

    M5.Display.fillRect(53, 471, 232, 53, TFT_BLACK);
    M5.Display.fillRect(320, 471, 250, 53, TFT_BLACK);
    writeText(53, 476, good ? String(ups.inputVoltTenths / 10.0f, 1) + " V" : "--",
              good ? TFT_WHITE : TFT_YELLOW, 3);
    writeText(320, 476, good ? String(ups.outputVoltTenths / 10.0f, 1) + " V" : "--",
              good ? TFT_WHITE : TFT_YELLOW, 3);

    M5.Display.fillRect(865, 210, 355, 49, TFT_BLACK);
    writeText(867, 215, aiBridgeChecked ?
             (aiBridgeOk ? "ONLINE" : "OFFLINE") : "SPRAWDZANIE",
             aiBridgeOk ? TFT_GREEN : TFT_YELLOW, 3);
    M5.Display.fillRect(865, 330, 355, 46, TFT_BLACK);
    writeText(867, 338, connected() ? WiFi.localIP().toString() :
              String("BRAK WI-FI"), connected() ? TFT_CYAN : TFT_YELLOW, 2);
    M5.Display.fillRect(865, 459, 355, 50, TFT_BLACK);
    writeText(867, 466, connected() ? String(WiFi.RSSI()) + " dBm" :
              String("BRAK SYGNALU"), TFT_WHITE, 2);
    M5.Display.fillRect(53, 547, 740, 26, TFT_BLACK);
    writeText(53, 550, "Zrodlo: MODBUS TCP 192.168.1.102:502 | FC03", TFT_LIGHTGREY, 2);
}

void drawDashboard() {
    header("PULPIT SYSTEMOWY   /   UPS + AI PLATFORM");
    M5.Display.drawRoundRect(32, 110, 788, 478, 14, TFT_DARKCYAN);
    M5.Display.drawRoundRect(844, 110, displayWidth - 876, 478, 14, TFT_DARKGREY);
    M5.Display.fillRect(34, 112, 784, 7, TFT_DARKCYAN);
    M5.Display.fillRect(846, 112, displayWidth - 880, 7, TFT_DARKGREEN);
    writeText(53, 138, "ZASILANIE AWARYJNE", TFT_CYAN, 3);
    writeText(53, 266, "BATERIA", TFT_LIGHTGREY);
    writeText(314, 266, "OBCIAZENIE", TFT_LIGHTGREY);
    writeText(565, 266, "CZAS UPS*", TFT_LIGHTGREY);
    M5.Display.drawFastHLine(54, 407, 733, TFT_DARKGREY);
    writeText(53, 428, "WEJSCIE", TFT_LIGHTGREY);
    writeText(320, 428, "WYJSCIE", TFT_LIGHTGREY);
    writeText(563, 428, "*szacowany czas", TFT_LIGHTGREY, 2);
    writeText(867, 145, "AI PLATFORM", TFT_CYAN, 3);
    writeText(867, 185, "AI Bridge /health", TFT_LIGHTGREY);
    M5.Display.drawFastHLine(867, 289, displayWidth - 906, TFT_DARKGREY);
    writeText(867, 302, "ADRES INFOTAB", TFT_LIGHTGREY);
    M5.Display.drawFastHLine(867, 414, displayWidth - 906, TFT_DARKGREY);
    writeText(867, 427, "SYGNAL WI-FI", TFT_LIGHTGREY);
    button(32, 608, 230, 82, "ODSWIEZ", TFT_DARKCYAN, 2);
    button(285, 608, 230, 82, "SIEC WIFI", TFT_DARKGREEN, 2);
    button(538, 608, 230, 82, "OTA WIFI", TFT_NAVY, 2);
    button(791, 608, 235, 82, "OTA DIAG", TFT_DARKGREY, 2);
    writeText(1040, 636, "v0.6", TFT_LIGHTGREY, 2);
    drawDashboardValues();
}

void refreshDashboard() {
    pollUpsReadOnly();
    pollAiBridge();
    lastDashboardRefresh = millis();
    if (page == Page::Dashboard) drawDashboardValues();
}

void handleDashboard(int x, int y) {
    if (inside(x, y, 32, 608, 230, 82)) {
        refreshDashboard();
    } else if (inside(x, y, 285, 608, 230, 82)) {
        page = Page::Home;
        render();
    } else if (inside(x, y, 538, 608, 230, 82)) {
        page = Page::OtaUpload;
        render();
    } else if (inside(x, y, 791, 608, 235, 82)) {
        page = Page::OtaDiag;
        infoMessage = "Odczyt partycji - bez zmiany Flash.";
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
    button(450, 557, 330, 67, "WROC DO PULPITU", TFT_DARKGREEN, 2);
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
    writeText(45, 116, "Firmware InfoTAB: v0.4-preflight", TFT_CYAN, 2);
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
        case Page::Home: drawHome(); break;
        case Page::Networks: drawNetworks(); break;
        case Page::Password: drawPassword(); break;
        case Page::Connecting: drawConnecting(); break;
        case Page::OtaDiag: drawOtaDiag(); break;
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
        page = Page::Dashboard;
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
        page = Page::Home;
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
    logEvent("v0.5 boot: dashboard + Wi-Fi + read-only OTA diagnostics");
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
    if (touch.wasClicked()) {
        const int x = touch.x;
        const int y = touch.y;
        switch (page) {
            case Page::Dashboard: handleDashboard(x, y); break;
            case Page::Home: handleHome(x, y); break;
            case Page::Networks: handleNetworks(x, y); break;
            case Page::Password: handlePassword(x, y); break;
            case Page::Connecting: handleConnecting(x, y); break;
            case Page::OtaDiag: handleOtaDiag(x, y); break;
        }
    }
    updateConnection();
    if (page == Page::Dashboard && connected() &&
        (lastDashboardRefresh == 0 ||
         millis() - lastDashboardRefresh >= DASH_REFRESH_MS)) {
        refreshDashboard();
    }
    if (page == Page::Dashboard && !connected() &&
        millis() - lastDashboardRefresh >= DASH_REFRESH_MS) {
        ups.valid = false;
        aiBridgeChecked = true;
        aiBridgeOk = false;
        lastDashboardRefresh = millis();
        drawDashboard();
    }
    if (page == Page::Home && millis() - lastHomeRefresh >= HOME_REFRESH_MS) {
        lastHomeRefresh = millis();
        render();
    }
    delay(8);
}
