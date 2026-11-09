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
constexpr const char* SERVICES_URL = "http://192.168.1.100:18092/services";
constexpr const char* NAS_URL = "http://192.168.1.100:18092/nas";

enum class Page { Dashboard, Resources, Power, NetworkInfo, Service, Home, Networks, Password, Connecting, OtaDiag, OtaUpload };
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
static constexpr size_t NAS_ROW_COUNT = 13;
const char* nasMetricKeys[NAS_ROW_COUNT] = {
    "nas_state", "snmp_state", "cpu_pct", "ram_pct",
    "temp_cpu_c", "temp_system_c", "disks", "raid", "volumes",
    "free_gib", "fan_rpm", "uptime_days", "backup"
};
const char* nasMetricLabels[NAS_ROW_COUNT] = {
    "Stan NAS", "Odczyt SNMP", "Obciazenie CPU", "Zajetosc RAM",
    "Temperatura CPU", "Temperatura systemu", "Dyski (status QTS)", "Stan RAID",
    "Stan wolumenow", "Wolne miejsce", "Wentylatory", "Czas pracy", "Backup"
};
String nasMetricValues[NAS_ROW_COUNT];
String nasLastDisplayed[NAS_ROW_COUNT];
uint16_t nasLastDisplayColor[NAS_ROW_COUNT]{};
bool nasRowsPainted = false;
bool nasApiAvailable = false;


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
    // No separate topbar, even in settings and OTA subpages.
    M5.Display.fillScreen(0x322A);
    M5.Display.setTextColor(0xFFFF, 0x322A);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(MARGIN, 24);
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
// Exact identifiers from server/infotab_services.py on the AI Platform.
// 13 fixed rows; never execute a command or accept arbitrary unit names.
enum class ServiceState : uint8_t { Unknown, Running, Stopped };
struct ServiceRow {
    const char* id;
    const char* label;
    ServiceState state;
    ServiceState lastDrawn;
};
ServiceRow aiServices[] = {
    {"bridge",       "AI Bridge",          ServiceState::Unknown, ServiceState::Unknown},
    {"gateway",      "Gateway / RM",       ServiceState::Unknown, ServiceState::Unknown},
    {"ollama",       "Ollama",             ServiceState::Unknown, ServiceState::Unknown},
    {"ollama_rtx",   "Ollama RTX",         ServiceState::Unknown, ServiceState::Unknown},
    {"postgres",     "PostgreSQL",         ServiceState::Unknown, ServiceState::Unknown},
    {"qdrant",       "Qdrant",             ServiceState::Unknown, ServiceState::Unknown},
    {"comfyui",      "ComfyUI",            ServiceState::Unknown, ServiceState::Unknown},
    {"radio",        "RADIO",              ServiceState::Unknown, ServiceState::Unknown},
    {"ups_guardian", "UPS Guardian",       ServiceState::Unknown, ServiceState::Unknown},
    {"ups_rm",       "UPS / RM",           ServiceState::Unknown, ServiceState::Unknown},
    {"hermes",       "Hermes",             ServiceState::Unknown, ServiceState::Unknown},
    {"magazyn_web",  "Magazyn WebGUI",     ServiceState::Unknown, ServiceState::Unknown},
    {"magazyn_bot",  "Magazyn Telegram",   ServiceState::Unknown, ServiceState::Unknown},
};
constexpr size_t AI_SERVICE_COUNT = sizeof(aiServices) / sizeof(aiServices[0]);
bool serviceRowsPainted = false;
bool serviceApiChecked = false;
bool serviceApiAvailable = false;

void pollPlatformServices() {
    serviceApiChecked = true;
    serviceApiAvailable = false;
    ServiceState next[AI_SERVICE_COUNT]{};
    for (size_t i = 0; i < AI_SERVICE_COUNT; ++i) next[i] = ServiceState::Unknown;

    if (connected()) {
        NetworkClient client;
        HTTPClient http;
        http.setConnectTimeout(1000);
        http.setTimeout(3500);
        if (http.begin(client, String(SERVICES_URL))) {
            if (http.GET() == 200) {
                String body = http.getString();
                bool complete = true;
                for (size_t i = 0; i < AI_SERVICE_COUNT; ++i) {
                    const String target = String(aiServices[i].id) + "=";
                    int index = body.indexOf(target);
                    if (index > 0 && body[index - 1] != '\n') index = -1;
                    if (index < 0) { complete = false; break; }
                    const int valueBegin = index + target.length();
                    int end = body.indexOf('\n', valueBegin);
                    if (end < 0) end = body.length();
                    String value = body.substring(valueBegin, end);
                    value.trim();
                    if (value == "running") next[i] = ServiceState::Running;
                    else if (value == "stopped") next[i] = ServiceState::Stopped;
                    else if (value == "unknown") next[i] = ServiceState::Unknown;
                    else { complete = false; break; }
                }
                serviceApiAvailable = complete;
            }
            http.end();
        }
    }
    for (size_t i = 0; i < AI_SERVICE_COUNT; ++i)
        aiServices[i].state = serviceApiAvailable ? next[i] : ServiceState::Unknown;
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

// Read-only fixed-key NAS data from AI Platform. No QNAP credentials on Tab5.
// Unknown metrics stay "--"; API failure is not interpreted as NAS failure.
void pollNas() {
    nasApiAvailable = false;
    for (size_t i = 0; i < NAS_ROW_COUNT; ++i) nasMetricValues[i] = "--";
    if (!connected()) return;

    NetworkClient client;
    HTTPClient http;
    http.setConnectTimeout(1000);
    http.setTimeout(7000);
    if (!http.begin(client, String(NAS_URL))) return;
    if (http.GET() == 200) {
        const String body = http.getString();
        bool complete = body.length() > 0 && body.length() < 2048;
        if (complete) {
            for (size_t i = 0; i < NAS_ROW_COUNT; ++i) {
                const String target = String(nasMetricKeys[i]) + "=";
                int start = body.indexOf(target);
                if (start > 0 && body[start - 1] != '\n') start = -1;
                if (start < 0) { complete = false; break; }
                const int valueStart = start + target.length();
                int lineEnd = body.indexOf('\n', valueStart);
                if (lineEnd < 0) lineEnd = body.length();
                if (lineEnd - valueStart > 45) { complete = false; break; }
                String value = body.substring(valueStart, lineEnd);
                value.trim();
                nasMetricValues[i] = value.length() ? value : "--";
            }
        }
        nasApiAvailable = complete;
    }
    http.end();
    if (!nasApiAvailable) {
        for (size_t i = 0; i < NAS_ROW_COUNT; ++i) nasMetricValues[i] = "--";
    }
}

// Static screen elements are drawn only when navigating to this page.
// Dynamic values refresh in small rectangles; NO fillScreen() on any timer.
// InfoTAB: restrained industrial HMI, 5 horizontal swipe pages.
// Only page changes clear the whole LCD. Telemetry changes repaint values.
static constexpr uint16_t UI_BG = 0x322A;    // brighter charcoal
static constexpr uint16_t UI_CARD = 0x532E;  // medium graphite
static constexpr uint16_t UI_BORDER = 0x7453;
static constexpr uint16_t UI_WHITE = 0xFFFF;
static constexpr uint16_t UI_MUTED = 0xD71D;
static constexpr uint16_t UI_ACCENT = 0x8678; // muted teal
static constexpr uint16_t UI_GOOD = 0x9EF6;
static constexpr uint16_t UI_WARNING = 0xFE93;

// A single source of truth for display order, swipe handling and tap targets.
static constexpr Page MAIN_PAGES[] = {
    Page::Dashboard, Page::Resources, Page::NetworkInfo,
    Page::Power, Page::Service
};
static constexpr int MAIN_PAGE_COUNT =
    static_cast<int>(sizeof(MAIN_PAGES) / sizeof(MAIN_PAGES[0]));
static constexpr int NAV_LEFT = 32;
static constexpr int NAV_RIGHT = 32;
static_assert(MAIN_PAGE_COUNT == 5, "InfoTAB navigation has five modules");

bool mainPage(Page p) {
    for (int i = 0; i < MAIN_PAGE_COUNT; ++i) {
        if (MAIN_PAGES[i] == p) return true;
    }
    return false;
}
int mainPageIndex(Page p) {
    for (int i = 0; i < MAIN_PAGE_COUNT; ++i) {
        if (MAIN_PAGES[i] == p) return i;
    }
    return -1;
}
Page mainPageByIndex(int idx) {
    return MAIN_PAGES[constrain(idx, 0, MAIN_PAGE_COUNT - 1)];
}
int navigationSlotWidth() {
    return (displayWidth - NAV_LEFT - NAV_RIGHT) / MAIN_PAGE_COUNT;
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
    static const char* labels[] = {
        "AI PLATFORM", "ZASOBY", "NAS", "UPS", "USTAWIENIA"
    };
    static_assert(sizeof(labels) / sizeof(labels[0]) == MAIN_PAGE_COUNT,
                  "InfoTAB page labels and page order must match");
    const int slot = navigationSlotWidth();
    for (int i = 0; i < MAIN_PAGE_COUNT; ++i) {
        const int x = NAV_LEFT + i * slot;
        const bool selected = active == i;
        if (selected)
            M5.Display.fillRoundRect(x + 3, 640, slot - 6, 60, 9, UI_CARD);
        M5.Display.setTextSize(2);
        const int labelWidth = M5.Display.textWidth(labels[i]);
        uiText(x + (slot - labelWidth) / 2, 663, labels[i],
               selected ? UI_WHITE : UI_MUTED, 2,
               selected ? UI_CARD : UI_BG);
        if (selected)
            M5.Display.fillRect(x + 14, 697, slot - 28, 3, UI_ACCENT);
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
// Flat industrial HMI list: static labels plus differential status updates.
void drawOverviewValues() {
    for (size_t i = 0; i < AI_SERVICE_COUNT; ++i) {
        ServiceRow &service = aiServices[i];
        if (serviceRowsPainted && service.state == service.lastDrawn) continue;
        const int y = 88 + static_cast<int>(i) * 39;
        M5.Display.fillRect(948, y + 3, 283, 31, UI_BG);
        uint16_t color = UI_MUTED;
        const char* stateText = "BRAK DANYCH";
        if (service.state == ServiceState::Running) {
            color = UI_GOOD;
            stateText = "URUCHOMIONA";
        } else if (service.state == ServiceState::Stopped) {
            color = UI_WARNING;
            stateText = "ZATRZYMANA";
        }
        M5.Display.fillCircle(967, y + 17, 6, color);
        uiText(994, y + 9, stateText, color, 2, UI_BG);
        service.lastDrawn = service.state;
    }
    serviceRowsPainted = true;
}
void drawOverview() {
    tabShell("", 0);
    serviceRowsPainted = false;
    uiText(42, 29, "USLUGI AI PLATFORM", UI_WHITE, 3, UI_BG);
    M5.Display.drawFastHLine(40, 74, 1193, UI_BORDER);
    for (size_t i = 0; i < AI_SERVICE_COUNT; ++i) {
        const int y = 88 + static_cast<int>(i) * 39;
        uiText(56, y + 9, aiServices[i].label, UI_WHITE, 2, UI_BG);
        M5.Display.drawFastHLine(48, y + 37, 1181, UI_BORDER);
    }
    drawOverviewValues();
}
// Resources is intentionally an empty module shell until its telemetry
// contract is agreed. No fabricated resource percentages or GPU load.
void drawResources() {
    tabShell("", 1);
    uiText(42, 29, "ZASOBY", UI_WHITE, 3, UI_BG);
    M5.Display.drawFastHLine(40, 74, displayWidth - 80, UI_BORDER);
    uiText(56, 116, "MONITORING ZASOBOW AI PLATFORM", UI_MUTED, 2, UI_BG);
    M5.Display.drawFastHLine(48, 169, displayWidth - 96, UI_BORDER);
    uiText(56, 195, "ODCZYTY NIE SA JESZCZE SKONFIGUROWANE", UI_MUTED, 2, UI_BG);
}

// Eight aligned tiles; each live measurement has its own light graphite card.
// UPS: the entire module is eight tiles. No topbar or outside diagnostics.
// UPS module uses a clear single-column list, consistent with AI Platform.
// Only changed right-column values are repainted during polling.
static constexpr size_t UPS_ROW_COUNT = 9;
String upsLastShown[UPS_ROW_COUNT];
uint16_t upsLastColor[UPS_ROW_COUNT]{};
bool upsListPainted = false;

void drawPowerValues() {
    String values[UPS_ROW_COUNT] = {
        !ups.valid ? "--" : (ups.modeCode == 4 ?
            String("NORMALNY") : String("TRYB ") + String(ups.modeCode)),
        ups.valid ? String(ups.inputVoltTenths / 10.0f, 1) + " V" : "--",
        ups.valid ? String(ups.outputVoltTenths / 10.0f, 1) + " V" : "--",
        ups.valid ? String(ups.outputHzTenths / 10.0f, 1) + " Hz" : "--",
        ups.valid && ups.powerWattsEstimate >= 0 ?
            String("~") + String(ups.powerWattsEstimate) + " W" : "--",
        ups.valid ? String(ups.batteryPct) + "%" : "--",
        ups.valid ? String(ups.loadPct) + "%" : "--",
        ups.valid ? String(ups.runtimeSeconds / 60) + " min" : "--",
        ups.valid && ups.batteryVoltTenths > 0 ?
            String(ups.batteryVoltTenths / 10.0f, 1) + " V" : "--"
    };
    for (size_t i = 0; i < UPS_ROW_COUNT; ++i) {
        uint16_t color = UI_WHITE;
        if (!ups.valid) color = UI_MUTED;
        else if (i == 0) color = upsStateColor();
        else if (i == 5) color = UI_GOOD;
        if (upsListPainted && values[i] == upsLastShown[i] &&
            color == upsLastColor[i]) continue;
        const int y = 92 + static_cast<int>(i) * 54;
        M5.Display.fillRect(903, y, 326, 40, UI_BG);
        uiText(943, y + 8, values[i], color, 2, UI_BG);
        upsLastShown[i] = values[i];
        upsLastColor[i] = color;
    }
    upsListPainted = true;
}

void drawPower() {
    tabShell("", 3);
    upsListPainted = false;
    static const char* labels[UPS_ROW_COUNT] = {
        "Stan UPS",
        "Napiecie wejsciowe",
        "Napiecie wyjsciowe",
        "Czestotliwosc wyjscia",
        "Moc wyjsciowa (szac.)",
        "Naladowanie baterii",
        "Obciazenie UPS",
        "Czas podtrzymania (szac.)",
        "Napiecie baterii"
    };
    uiText(42, 29, "PARAMETRY UPS", UI_WHITE, 3, UI_BG);
    M5.Display.drawFastHLine(40, 74, 1193, UI_BORDER);
    for (size_t i = 0; i < UPS_ROW_COUNT; ++i) {
        const int y = 92 + static_cast<int>(i) * 54;
        uiText(56, y + 8, labels[i], UI_WHITE, 2, UI_BG);
        M5.Display.drawFastHLine(48, y + 46, 1181, UI_BORDER);
    }
    uiText(42, 583, "ZRODLO: MODBUS TCP 192.168.1.102:502  |  MOC: WARTOSC ORIENTACYJNA",
           UI_MUTED, 2, UI_BG);
    drawPowerValues();
}

// NAS: flat high-contrast list, differential repaints only.
void drawNetworkValues() {
    for (size_t i = 0; i < NAS_ROW_COUNT; ++i) {
        String value = nasMetricValues[i];
        uint16_t color = UI_WHITE;
        if (!nasApiAvailable) {
            value = i == 0 ? "BRAK API" : "--";
            color = UI_MUTED;
        } else if (i == 0) {
            color = value == "online" ? UI_GOOD : UI_WARNING;
            value = value == "online" ? "ONLINE" : "OFFLINE";
        } else if (i == 1) {
            if (value == "ok") { value = "AKTYWNY"; color = UI_GOOD; }
            else if (value == "not_configured") { value = "DO KONFIGURACJI"; color = UI_WARNING; }
            else if (value == "config_error") { value = "BLAD KONFIG."; color = UI_WARNING; }
            else { value = "BRAK ODCZYTU"; color = UI_WARNING; }
        } else if (value == "--") {
            color = UI_MUTED;
        } else if (i == 6 || i == 8) {
            const int slash = value.indexOf('/');
            const int space = value.indexOf(' ');
            if (slash > 0 && space > slash + 1) {
                const int good = value.substring(0, slash).toInt();
                const int total = value.substring(slash + 1, space).toInt();
                color = total > 0 && good == total ? UI_GOOD : UI_WARNING;
            }
        } else if (i == 2 || i == 3) value += "%";
        else if (i == 4 || i == 5) value += " C";
        else if (i == 9) value += " GiB";
        else if (i == 10) value += " RPM";
        else if (i == 11) value += " dni";

        if (nasRowsPainted && value == nasLastDisplayed[i] &&
            color == nasLastDisplayColor[i]) continue;
        const int y = 86 + static_cast<int>(i) * 40;
        M5.Display.fillRect(901, y + 3, 328, 31, UI_BG);
        M5.Display.fillCircle(919, y + 18, 5, color);
        uiText(942, y + 8, value, color, 2, UI_BG);
        nasLastDisplayed[i] = value;
        nasLastDisplayColor[i] = color;
    }
    nasRowsPainted = true;
}
void drawNetwork() {
    tabShell("", 2);
    nasRowsPainted = false;
    uiText(42, 29, "PARAMETRY NAS", UI_WHITE, 3, UI_BG);
    M5.Display.drawFastHLine(40, 74, 1193, UI_BORDER);
    for (size_t i = 0; i < NAS_ROW_COUNT; ++i) {
        const int y = 86 + static_cast<int>(i) * 40;
        uiText(56, y + 8, nasMetricLabels[i], UI_WHITE, 2, UI_BG);
        M5.Display.drawFastHLine(48, y + 39, 1181, UI_BORDER);
    }
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
    valueAt(57, 427, 540, 44, "InfoTAB v0.9.3", UI_WHITE, 3);
    valueAt(57, 509, 540, 39,
            String("Czas pracy: ") + String(millis() / 60000) + " min", UI_MUTED, 2);
}
void drawService() {
    tabShell("", 4);
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
    if (page == Page::Dashboard) pollPlatformServices();
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
    const int slot = navigationSlotWidth();
    for (int i = 0; i < MAIN_PAGE_COUNT; ++i) {
        const int left = NAV_LEFT + i * slot;
        if (inside(x, y, left, 632, slot, 76)) {
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
void handleResources(int x, int y) {
    handleNavigationTap(x, y);
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
    header("DIAGNOSTYKA OTA");
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    const uint32_t sketchBytes = ESP.getSketchSize();
    writeText(45, 116, "Firmware InfoTAB: v0.9.2", TFT_CYAN, 2);
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
        case Page::Resources: drawResources(); break;
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
    logEvent("v0.9.2: UPS and AI Platform list views, Modbus and OTA");
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
            if (next >= 0 && next < MAIN_PAGE_COUNT) {
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
            case Page::Resources: handleResources(x, y); break;
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
