# InfoTAB — projekt bezpiecznej aktualizacji OTA

Status: **v0.4-preflight — test sprzętowy PASS dla odczytu partycji i HTTP 200**. Bez mechanizmu aktualizacji OTA. **STOP dla pierwszej aktualizacji OTA** do rozstrzygnięcia rozbieżności w tabeli partycji.
Data: 2026-11-06.

## Stan potwierdzony na sprzęcie

- Tab5 ESP32-P4 + ESP32-C6 SDIO, Windows 11 / PlatformIO
- Ekran i dotyk działają
- Konfiguracja Wi-Fi na ekranie, zapis NVS i reconnect po restarcie: **PASS**
- Odczytany lokalny adres IP z DHCP: **192.168.1.38**
- v0.4-preflight: odczyt na urządzeniu i komunikacja z AI Bridge **PASS** (zdjęcia 2026-11-06)
- Bieżący firmware: 932384 B, aktywna `app0` offset 0x10000/1280 KiB,
  następny slot `app1` offset 0x150000/1280 KiB.
- Zgodność boot/running: PASS. Rollback: NIEPRZETESTOWANY.
- Wprowadzenie zapisu OTA: **BLOCKED — wymagany audyt rzeczywistej tabeli partycji**.

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

**UWAGA: dane z urządzenia przeczą temu domyślnemu ustawieniu.**
Na zdjęciu diagnostyki v0.4 z 2026-11-06 widać **faktyczną, uruchomioną**
tabelę: `app0 0x10000 0x140000` i `app1 0x150000 0x140000`,
czyli **dwa sloty po 1280 KiB (1,25 MiB)**. Odpowiada to layoutowi
Arduino `default.csv` dla 4 MiB Flash, a NIE layoutowi `default_16MB.csv`.
Różnica jest istotna: 16 MB na układzie nie gwarantuje 16 MB
w poprawnie skonfigurowanej tabeli.

**Nie zmieniamy automatycznie `board_build.partitions` i nie wykonujemy
nowego USB Upload w celu przebudowy tabeli.** Najpierw porównujemy
wygenerowany lokalnie `.pio/build/infotab_tab5/partitions.bin`
za pomocą narzędzia `scripts/audit-partitions.py` (TYLKO ODCZYT),
a dopiero później podejmujemy decyzję o migracji, backupie NVS
i bezpiecznym reprovisioningu przez USB-C.

Rozmiar 932384 B pozostawia ok. 369 KiB zapasu w każdym slocie,
co dla planowanego interfejsu i integracji UPS jest ograniczeniem rozwojowym.
Nie traktujemy samego faktu, że A/B istnieje, jako zgody na OTA.

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
4. Otworzyć `OTA DIAG (ODCZYT)` — **PASS: app0/app1 po 1280 KiB**.
5. Nacisnąć `TEST AI BRIDGE` — **PASS: HTTP 200 (ONLINE)**.
6. Wcisnąć `WROC`, potwierdzić działanie konfiguracji Wi-Fi.
7. Na komputerze Windows 11 wykonać po Build odczyt lokalnej tabeli:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\scripts\audit-partitions.py
```

Wynik porównać ze zdjęciem. Jeśli lokalny layout ma 6,25 MiB,
a urządzenie 1,25 MiB, nie wgrywać kolejnego firmware bez planu
migracji i zabezpieczenia NVS.

Jeśli Build nie przejdzie, **nie wykonywać Upload**.
Obecne sloty OTA są małe; **nie uruchamiać OTA i nie modyfikować tabeli partycji**, dopóki nie zweryfikujemy lokalnego obrazu partycji oraz metody ochrony NVS.
Nie podawać w logach haseł sieci ani prywatnych tokenów.

## Audyt 16 MB — odrębny wariant kompilacji (PENDING)

Audyt lokalnej tabeli potwierdzony przez użytkownika 2026-11-06:

- `nvs`: 0x9000 / 0x5000 (20 KiB)
- `otadata`: 0xE000 / 0x2000 (8 KiB)
- `app0`: 0x10000 / 0x140000 (1280 KiB)
- `app1`: 0x150000 / 0x140000 (1280 KiB)
- `firmware.bin`: 932384 B, wolne 378336 B w każdym slocie

Wniosek: **zarówno firmware na urządzeniu, jak i lokalny build używają
4 MB `default.csv`**. Board manifest z `default_16MB.csv` nie
wymusił większej tabeli w użytym środowisku PlatformIO.

Przygotowano osobny, NIEdomyślny wariant kompilacji:
`[env:infotab_tab5_16mb_audit]`, który dziedziczy istniejącą konfigurację
i jawnie wskazuje `partitions/infotab_16mb.csv`.
Główny `[env:infotab_tab5]` pozostaje bez zmian.

Dwie partycje w nowym wariancie mają po 0x640000 B (6,25 MiB).
NVS zachowuje ten sam offset i rozmiar co dotychczas, ale **nie uznajemy
tego za gwarancję zachowania danych podczas migracji**.

Na Windows 11, wewnątrz folderu repo:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-16mb-audit-windows.ps1
```

Skrypt kompiluje tylko kandydata i analizuje `partitions.bin`, bez
podłączania do Tab5, zapisu Flash ani wykonywania Upload.
Oczekiwany komunikat:
`16MB BUILD + AUDIT PASS. USB UPLOAD WAS NOT EXECUTED.`

**Przed migracją:** zarchiwizować lokalnie binarną kopię NVS
(SSID/hasło — traktować jako sekret), przeanalizować aktualny `otadata`,
sprawdzić faktyczną pojemność Flash, zapisać plan powrotu przez USB-C,
oraz wyznaczyć procedurę flashowania tabeli + app0 bez `erase_flash`.
Nie kopiować prywatnego NVS na GitHub ani AI Platform.
Weryfikacja kandydata nie upoważnia do flashowania.

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
2. Odczyt `ESP.getSketchSize()`, partycji uruchomionej i następnego slotu na Tab5: **PASS** (932384 B, app0/app1 po 1280 KiB).
   Dodatkowo wymagana jest weryfikacja lokalnego `partitions.bin` przez skrypt audytowy.
3. Przegląd endpointu serwującego firmware z AI Platform i polityki auth/TLS.
4. Zdefiniowanie podpisu firmware, wersjonowania i testu odmowy
   nieautoryzowanej aktualizacji.
5. Ścieżka odtworzenia aktualnego v0.3 przez USB-C dostępna i przetestowana.

**Uwaga:** na tym etapie nie instalujemy nowego bootloadera,
nie nadpisujemy partycji i nie zmieniamy danych NVS. Modyfikacja układu
partycji wymaga osobnego planu migracji przez USB-C, z kopią konfiguracji
Wi-Fi i sprawdzoną ścieżką odzyskania.
