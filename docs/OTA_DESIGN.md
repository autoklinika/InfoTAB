# InfoTAB — projekt bezpiecznej aktualizacji OTA

Status: **v0.4-preflight — kod diagnostyczny przygotowany**, kompilacja i uruchomienie na sprzęcie: PENDING. Bez mechanizmu aktualizacji OTA. Działający firmware v0.3 na Tab5 pozostaje bez zmian do następnego wgrania przez USB.
Data: 2026-11-06.

## Stan potwierdzony na sprzęcie

- Tab5 ESP32-P4 + ESP32-C6 SDIO, Windows 11 / PlatformIO
- Ekran i dotyk działają
- Konfiguracja Wi-Fi na ekranie, zapis NVS i reconnect po restarcie: **PASS**
- Odczytany lokalny adres IP z DHCP: **192.168.1.38**
- Wprowadzenie OTA: **PENDING**

## Już istniejące partycje OTA (nie zmieniamy pochopnie tabeli)

Wersja `pioarduino/platform-espressif32#54.03.21` wskazuje
`boards/esp32-p4-evboard.json`, z wpisem
`arduino.partitions = default_16MB.csv` (Arduino core 3.2.1).

Oficjalna tabela `tools/partitions/default_16MB.csv` w Arduino ESP32 3.2.1:

| Partition | Offset | Size |
| --- | --- | --- |
| nvs | 0x9000 | 0x5000 |
| otadata | 0xE000 | 0x2000 |
| app0 (ota_0) | 0x10000 | 0x640000 = 6.25 MiB |
| app1 (ota_1) | 0x650000 | 0x640000 = 6.25 MiB |
| spiffs | 0xC90000 | 0x360000 |
| coredump | 0xFF0000 | 0x10000 |

Oznacza to, że *źródłowa* konfiguracja przewiduje A/B OTA,
bez potrzeby zmiany tabeli przed pierwszym testem.
Trzeba potwierdzić rzeczywisty aktywny slot i rozmiar binarki
na docelowym urządzeniu — sama deklaracja w repo nie gwarantuje
zachowania działającego bootloadera po innych wcześniejszych flashach.

Źródła:
- https://github.com/pioarduino/platform-espressif32/blob/54.03.21/boards/esp32-p4-evboard.json
- https://github.com/espressif/arduino-esp32/blob/3.2.1/tools/partitions/default_16MB.csv
- https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html

## v0.4-preflight — diagnostyka na ekranie

Nowy ekran `OTA DIAG (ODCZYT)` w ustawieniach InfoTAB pokazuje:
- wersję diagnostyczną i wielkość aktualnego obrazu aplikacji;
- aktywną partycję oraz jej rozmiar i offset;
- partycję wybraną jako następny slot aktualizacji;
- zgodność bieżącego slotu rozruchowego z działającą aplikacją;
- status Wi-Fi oraz opcjonalny test HTTP 200 do `http://192.168.1.100:8080/health`.

Wszystkie odczyty partycji wykorzystują API ESP-IDF (`esp_ota_get_running_partition`, `esp_ota_get_next_update_partition` i `esp_ota_get_boot_partition`).
**Nie ma wywołań `Update.begin`, `esp_ota_begin`, `esp_ota_set_boot_partition` ani zapisu do Flash**; nie uruchomimy jeszcze OTA.

Test HTTP dotyczy wyłącznie `/health`; nie jest przesyłaniem firmware.
Weryfikacja wykonana z hosta AI Platform: endpoint `/health` zwraca HTTP 200, a jego OpenAPI
nie zawiera endpointu do aktualizacji InfoTAB. Należy przygotować odrębny, minimalny
serwis OTA z kontrolą dostępu — nie rozszerzać bez potrzeby AI Bridge.

### Test odbiorczy na Tab5

1. `git pull --ff-only`, następnie lokalny `scripts/build-windows.ps1`.
2. Po `BUILD SUCCESS` wgrać przez USB-C/COM6.
3. Po uruchomieniu sprawdzić autołączenie z zapamiętaną siecią.
4. Otworzyć `OTA DIAG (ODCZYT)`; przepisać nazwy i rozmiary aktywnego i zapasowego slotu.
5. Nacisnąć `TEST AI BRIDGE`, oczekiwane: `AI BRIDGE: HTTP 200 (ONLINE)`.
6. Wcisnąć `WROC`, potwierdzić działanie konfiguracji Wi-Fi.

Jeśli Build nie przejdzie, **nie wykonywać Upload**.
Jeśli OTA slot jest niedostępny lub za mały, bez diagnozy nie instalować nowych obrazów.
Nie podawać w logach haseł sieci ani prywatnych tokenów.

## Architektura docelowa (propozycja)

- Punkt udostępniający firmware: serwer lokalny AI Platform (`192.168.1.100`);
  bez wystawiania usługi OTA do Internetu.
- InfoTAB po Wi-Fi pobiera informację o nowej wersji z API serwera.
- Aktualizacja jest uruchamiana **ręcznie na ekranie**, nie automatycznie
  po wykryciu pliku.
- Weryfikacja autentyczności firmware (podpis + zaufany klucz),
  TLS/HTTPS z certyfikatem kontrolowanym lokalnie.
- Pobieranie strumieniowe do nieaktywnej partycji OTA.
- Sprawdzenie długości obrazu i SHA-256 przed zmianą slotu rozruchowego;
  sam hash nie zastępuje podpisu cyfrowego.
- Po aktualizacji ekran pokazuje numer wersji i wynik autotestu.
- Zachować działający tryb recovery przez kabel USB-C/COM6.
- Rollback automatyczny dopiero po potwierdzeniu wsparcia
  bootloadera i flag ESP-IDF, nie zakładać go na podstawie samego A/B.
- Nie logować haseł ani tokenów; brak sekretów w GitHub.

## Warunki wejścia do implementacji

1. Rezerwacja DHCP w UniFi dla urządzenia, sugerowany aktualny IP 192.168.1.38.
   Adres jest informacją lokalną i nie powinien być zakodowany na stałe
   w firmware OTA (preferować konfigurację lub DNS).
2. Odczyt `ESP.getSketchSize()`, partycji uruchomionej i następnego slotu
   na docelowym Tab5 (test read-only).
3. Przegląd endpointu serwującego firmware z AI Platform i polityki auth/TLS.
4. Zdefiniowanie podpisu firmware, wersjonowania i testu odmowy
   nieautoryzowanej aktualizacji.
5. Ścieżka odtworzenia aktualnego v0.3 przez USB-C dostępna i przetestowana.

**Uwaga:** na tym etapie nie instalujemy nowego bootloadera,
nie nadpisujemy partycji i nie zmieniamy danych NVS.
