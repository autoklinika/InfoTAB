# InfoTAB

Oprogramowanie wyświetlacza informacyjnego InfoTAB na M5Stack Tab5 (ESP32-P4).

## Status

v0.1: kompilacja i uruchomienie przez COM6 potwierdzone. **v0.2: PASS** dla dotyku i skanowania Wi-Fi na rzeczywistym Tab5. **v0.3: połączenie z Wi-Fi skonfigurowane na ekranie — PASS pierwszego połączenia (2026-11-06).** Automatyczne połączenie po restarcie — PASS, aktualny IP DHCP 192.168.1.38. v0.4-preflight: test sprzętowy PASS; aktywna app0 i zapasowa app1 mają po 1280 KiB, boot=running, AI Bridge HTTP 200. Aktualizacje OTA i integracja UPS nie są jeszcze wdrożone.

**Zasada:** gałąź `main` pozostaje stabilna; zmiany z gałęzi roboczych są scalane wyłącznie po zatwierdzeniu.

## Sprzęt

- M5Stack Tab5 (ESP32-P4 + ESP32-C6)
- Komputer programujący: Windows 11, VS Code + PlatformIO
- Pierwsze programowanie: USB-C / COM6
- Późniejszy etap: aktualizacje OTA przez Wi-Fi

## Instrukcja

Przejdź na gałąź `feature/initial-platformio-tab5` i przeczytaj `docs/WINDOWS11.md`.

## Instalacja i test InfoTAB v0.3 (Windows 11)

W folderze repozytorium na gałęzi roboczej:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Jeżeli Build kończy się `SUCCESS`, wgraj przez USB-C / COM6:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

Po poprawnym flashu naciśnij krótko boczny przycisk zasilania,
jeśli ekran jest czarny. Otwórz ekran **WYBIERZ SIEC**, wybierz SSID
i wpisz hasło na urządzeniu.

Pełny scenariusz testowy i zasady bezpieczeństwa:
[docs/WIFI_SETUP.md](docs/WIFI_SETUP.md).

Logowanie przez COM6, 115200 bps (zamknij inne aplikacje zajmujące port):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM6 -b 115200
```

Dokumentacja Wi-Fi i pinów SDIO: https://docs.m5stack.com/en/arduino/m5tab5/wifi
Dotyk: https://docs.m5stack.com/en/arduino/m5unified/touch_class

## InfoTAB v0.4-preflight (read-only)

Na ekranie głównym wybierz `OTA DIAG (ODCZYT)`. Wyświetli aktywną partycję,
zapasowy slot oraz rozmiar firmware. Przycisk `TEST AI BRIDGE` wykona tylko
GET na `192.168.1.100:8080/health`. Żadnych zapisów do Flash ani aktualizacji OTA.

Instrukcja i weryfikacja: [docs/OTA_DESIGN.md](docs/OTA_DESIGN.md).

Aby przetestować nowy obraz na Windows 11:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Po BUILD SUCCESS wgrywaj przez dotychczasowy port COM6. Do zatwierdzenia
testu sprzętowego zachowujemy możliwość powrotu do działającej v0.3.

## Nowy wariant 16 MB — wyłącznie kompilacja testowa

Audyt potwierdził, że obie istniejące partycje OTA na Tab5 i w
domyślnym Build mają po 1280 KiB. Poniższe polecenie sprawdza nowy,
**osobny wariant 16 MB** (sloty po 6,25 MiB); nie wgrywa firmware:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-16mb-audit-windows.ps1
```

Oczekiwany rezultat:
`16MB BUILD + AUDIT PASS. USB UPLOAD WAS NOT EXECUTED.`

**Nie uruchamiaj `-t upload` dla `infotab_tab5_16mb_audit`!**
Najpierw musimy wykonać bezpieczną lokalną kopię NVS,
sprawdzić konfigurację bootloadera i opracować migrację z planem odzyskania.

## Weryfikacja partycji przed OTA

**STOP: nie wdrażaj jeszcze firmware OTA i nie zmieniaj `board_build.partitions`.**
Sprzętowa diagnostyka pokazała sloty po 1280 KiB (układ 4 MB),
mimo że Tab5 ma 16 MB Flash. Audyt bez ingerencji w urządzenie:

```powershell
git pull --ff-only
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\scripts\audit-partitions.py
```

Jeśli nie ma `partitions.bin`, najpierw wykonaj zwykły Build
(`scripts/build-windows.ps1`). **Na razie bez Upload.**

## Backup Flash przed migracją do 16 MB (TYLKO ODCZYT)

**Flash ID PASS**: ESP32-P4 rev. 1.3, fizyczne 16 MB Flash (2026-11-06).
Wariant 16 MB skompilowano i sprawdzono, ale na urządzeniu nadal
obowiązuje działający układ 4 MB.

Przed zmianą partycji wykonaj **lokalny** backup 16 MB przez COM6:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\backup-flash-windows.ps1
```

Backup jest zapisywany do `%LOCALAPPDATA%\InfoTAB\FlashBackups`,
poza GitHubem. Skrypt sprawdza SHA-256, wielkość zrzutu i oryginalne
partycje bez odczytywania haseł w terminalu.

**Plik .bin może zawierać hasło Wi-Fi — nie przesyłaj go do czatu, GitHuba
ani publicznej chmury.** Stosuj szyfrowanie dysku Windows (BitLocker).

Pełna instrukcja: [docs/FLASH_BACKUP.md](docs/FLASH_BACKUP.md).

Do czasu zatwierdzenia backupu: **nie wgrywamy wariantu 16 MB i nie włączamy OTA**.

## Migracja OTA — preflight bez zapisu do Flash

Pełny backup 16 MiB zakończony `BACKUP VERIFIED - READ ONLY PASS` (2026-11-06).
Fizyczna pojemność Flash 16 MB i kandydacki build po 6,25 MiB: PASS.

Teraz sprawdź stan zabezpieczeń chipu i nakładanie dawnych partycji na
nowy layout, korzystając wyłącznie z prywatnej kopii lokalnej:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\preflight-migration-windows.ps1
```

Skrypt nie czyta haseł Wi-Fi w postaci tekstowej i nie zapisuje Flash.
Prześlij jedynie wynik tekstowy preflight, nigdy kopię 16 MiB.
Migracja USB i OTA pozostają ZABLOKOWANE do przejrzenia wyników.

## Preflight migracji 16 MB — zweryfikowany na urządzeniu

Odczyt ROM/eFuse zakończony: Secure Boot **Disabled**,
Flash Encryption **Disabled**, a `SPI_BOOT_CRYPT_CNT=0` (2026-11-06).
Pełny backup jest poprawny, ale stare `app1`, `spiffs` i
`coredump` mają niepuste dane w obszarach zajmowanych
przez nową partycję `app0`.

**Migracja pozostaje wstrzymana.** Plan i warunki bezpieczeństwa:
[docs/MIGRATION_16MB_PLAN.md](docs/MIGRATION_16MB_PLAN.md).

Nie wykonuj Upload nowej tabeli przed identyfikacją danych i przygotowaniem
sprawdzonej ścieżki przywracania.
