#include <Arduino.h>
#include <cstring>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// InfoTAB v0.5: working UPS dashboard (NUT read-only) + AI Platform status,
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
constexpr const char* UPS_HOST = "192.168.1.101";
constexpr uint16_t UPS_PORT = 3493;
constexpr const char* UPS_NAME = "qnapups";
constexpr uint32_t DASH_REFRESH_MS = 8000;
constexpr const char* BRIDGE_HEALTH = "http://192.168.1.100:8080/health";

enum class Page { Dashboard, Home, Networks, Password, Connecting, OtaDiag };
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
    bool onBattery = false;
    bool lowBattery = false;
    bool charging = false;
    int batteryPct = -1;
    int loadPct = -1;
    long runtimeSeconds = -1;
    uint32_t updatedAt = 0;
};
UpsData ups;

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


bool readNutVar(NetworkClient& client, const char* variable, String& result) {
    const String prefix = String("VAR ") + UPS_NAME + " " + variable + " ";
    client.print(String("GET VAR ") + UPS_NAME + " " + variable + "\n");
    String line = client.readStringUntil('\n');
    line.trim();
    if (!line.startsWith(prefix)) return false;
    String quoted = line.substring(prefix.length());
    if (quoted.length() < 2 || quoted[0] != '"' ||
        quoted[quoted.length() - 1] != '"') return false;
    result = quoted.substring(1, quoted.length() - 1);
    return true;
}

bool positiveNumber(const String& s, long& out) {
    if (!s.length() || s.length() > 10) return false;
    for (size_t i = 0; i < s.length(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    out = s.toInt();
    return out >= 0;
}

void pollUpsReadOnly() {
    UpsData sample;
    if (!connected()) { ups = sample; return; }
    NetworkClient client;
    client.setTimeout(800);
    if (!client.connect(UPS_HOST, UPS_PORT, 1200)) {
        ups = sample;
        logEvent("UPS NUT unavailable");
        return;
    }
    String status, battery, load, runtime;
    const bool ok =
        readNutVar(client, "ups.status", status) &&
        readNutVar(client, "battery.charge", battery) &&
        readNutVar(client, "ups.load", load) &&
        readNutVar(client, "battery.runtime", runtime);
    client.stop();
    long batValue = -1, loadValue = -1, runtimeValue = -1;
    if (!ok || !positiveNumber(battery, batValue) ||
        !positiveNumber(load, loadValue) ||
        !positiveNumber(runtime, runtimeValue) ||
        batValue > 100 || loadValue > 100) {
        ups = sample;
        logEvent("UPS NUT invalid or incomplete telemetry");
        return;
    }
    const String flags = " " + status + " ";
    sample.onBattery = flags.indexOf(" OB ") >= 0;
    const bool onLine = flags.indexOf(" OL ") >= 0;
    if (sample.onBattery == onLine) {
        ups = sample;
        logEvent("UPS NUT contradictory or absent OL/OB");
        return;
    }
    sample.lowBattery = flags.indexOf(" LB ") >= 0;
    sample.charging = flags.indexOf(" CHRG ") >= 0;
    sample.batteryPct = static_cast<int>(batValue);
    sample.loadPct = static_cast<int>(loadValue);
    sample.runtimeSeconds = runtimeValue;
    sample.updatedAt = millis();
    sample.valid = true;
    ups = sample;
    Serial.printf("[InfoTAB] UPS_NUT status=%s battery=%d%% load=%d%% runtime=%ldsec\n",
                  sample.onBattery ? "BATTERY" : "MAINS", sample.batteryPct,
                  sample.loadPct, sample.runtimeSeconds);
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

void drawDashboard() {
    header("Pulpit  |  UPS + AI Platform");
    const uint16_t statusColor = !ups.valid ? TFT_YELLOW :
                                 ups.onBattery ? TFT_RED : TFT_GREEN;
    M5.Display.drawRoundRect(32, 112, 792, 464, 14, TFT_DARKGREY);
    M5.Display.drawRoundRect(846, 112, displayWidth - 878, 464, 14, TFT_DARKGREY);
    writeText(53, 131, "ZASILANIE UPS", TFT_CYAN, 3);
    writeText(54, 184, !ups.valid ? "BRAK ODCZYTU NUT" :
              ups.onBattery ? "AWARIA ZASILANIA - BATERIA" :
              "ZASILANIE SIECIOWE OK", statusColor, 2);

    if (ups.valid) {
        writeText(55, 255, "BATERIA", TFT_LIGHTGREY, 2);
        writeText(55, 288, String(ups.batteryPct) + "%", TFT_GREEN, 5);
        writeText(360, 255, "OBCIAZENIE UPS", TFT_LIGHTGREY, 2);
        writeText(360, 293, String(ups.loadPct) + "%", TFT_WHITE, 4);
        writeText(55, 390, "PODTRZYMANIE (SZACUNEK UPS)", TFT_LIGHTGREY, 2);
        writeText(55, 426, String(ups.runtimeSeconds / 60) + " min", TFT_CYAN, 4);
        if (ups.lowBattery)
            writeText(55, 520, "ALARM: NISKI POZIOM BATERII", TFT_RED, 2);
        else if (ups.charging)
            writeText(55, 520, "Akumulator: ladowanie", TFT_GREEN, 2);
        else
            writeText(55, 520, "Dane z QNAP NUT", TFT_LIGHTGREY, 2);
    } else {
        writeText(55, 270, "Sprawdz polaczenie z QNAP", TFT_YELLOW, 2);
        writeText(55, 313, "192.168.1.101:3493", TFT_WHITE, 2);
        writeText(55, 363, "Nie pokazujemy nieaktualnych wartosci.", TFT_LIGHTGREY);
    }
    writeText(867, 136, "AI PLATFORM", TFT_CYAN, 3);
    writeText(867, 196, aiBridgeChecked ? (aiBridgeOk ? "ONLINE" : "OFFLINE") :
              "SPRAWDZANIE", aiBridgeOk ? TFT_GREEN : TFT_YELLOW, 3);
    writeText(867, 257, "192.168.1.100", TFT_WHITE, 2);
    writeText(867, 306, "AI Bridge /health", TFT_LIGHTGREY, 2);
    writeText(867, 375, "INFO TAB / SIEC", TFT_CYAN, 2);
    writeText(867, 423, connected() ? WiFi.localIP().toString() :
              String("BRAK WI-FI"), connected() ? TFT_GREEN : TFT_YELLOW, 2);
    writeText(867, 474, connected() ? String(WiFi.RSSI()) + " dBm" :
              String("Wybierz SIEC WIFI"), TFT_LIGHTGREY, 2);
    button(32, 608, 264, 85, "ODSWIEZ", TFT_DARKCYAN, 2);
    button(316, 608, 286, 85, "SIEC WIFI", TFT_DARKGREEN, 2);
    button(622, 608, 300, 85, "OTA DIAG", TFT_NAVY, 2);
    writeText(960, 635, "InfoTAB v0.5", TFT_LIGHTGREY, 2);
}

void refreshDashboard() {
    pollUpsReadOnly();
    pollAiBridge();
    lastDashboardRefresh = millis();
    if (page == Page::Dashboard) drawDashboard();
}

void handleDashboard(int x, int y) {
    if (inside(x, y, 32, 608, 264, 85)) {
        refreshDashboard();
    } else if (inside(x, y, 316, 608, 286, 85)) {
        page = Page::Home;
        render();
    } else if (inside(x, y, 622, 608, 300, 85)) {
        page = Page::OtaDiag;
        infoMessage = "Odczyt partycji - bez aktualizacji OTA.";
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
