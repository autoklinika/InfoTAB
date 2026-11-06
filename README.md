# InfoTAB

## InfoTAB v0.8 — jaśniejsze HMI i kafelki UPS

- Jaśniejsza grafitowo-niebieska kolorystyka na wszystkich ekranach.
- Ekran UPS: osiem osobnych kafelków (wejście/wyjście/częstotliwość/
  moc*, bateria/obciążenie/podtrzymanie/napięcie akumulatora).
- Moc wyjściowa *orientacyjna*, z prefiksem `~`, wymaga kalibracji.
- Gesty, Modbus TCP, Wi-Fi i uwierzytelnione OTA zachowane.
- **Nie zmieniamy tabeli partycji Flash.**

Przejrzyj na urządzeniu po aktualizacji OTA:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Przycisk OTA na InfoTAB: **SERWIS → OTA WIFI → AKTYWUJ OTA**.
Wpisz lokalnie na Windows jednorazowe hasło z ekranu.

Opis: [docs/HMI_V08.md](docs/HMI_V08.md).


## v0.8 — jaśniejsze HMI / kafelki UPS / OTA bez hasła

Przygotowany kod: 8 kafelków na stronie UPS, jaśniejsza paleta,
pobór mocy wyjściowej oznaczony jako **szacunek, do potwierdzenia skali**
oraz odbiornik OTA aktywowany dotykiem na **2 minuty bez hasła**.
W tej fazie to rozwiązanie **wyłącznie dla zaufanej sieci LAN**,
bez gwarancji ochrony przed innymi klientami w tej samej sieci.

**Pierwsze przejście z v0.7 do v0.8:** użyj ostatni raz znanego
uploadera z hasłem `scripts/upload-wifi-windows.ps1`, ponieważ
obecne firmware nadal wymaga hasła.

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-powershell-syntax.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Po uruchomieniu InfoTAB v0.8 kolejne OTA bez hasła:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Kompilacja i odbiór sprzętowy nowej wersji: **PENDING**.
[Dokumentacja v0.8](docs/INFOTAB_V08.md).
Poprzedni stan źródeł: `stable/v0.6-hardware-pass` i
dotychczasowe zatwierdzone wydanie na urządzeniu.

---


## InfoTAB v0.7 — HMI z gestami

Nowy interfejs: cztery ekrany **START / UPS / SIEC-AI / SERWIS**,
gest lewo/prawo, dolna nawigacja i odświeżanie samych wartości
pomiarowych. Zachowane: działający Modbus TCP, konfiguracja Wi-Fi,
AI Bridge i OTA.

**Instalacja przez Wi-Fi** (jednorazowy kod aktywuj na Tab5 dopiero
gdy uploader poprosi o hasło):

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-powershell-syntax.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Nowe HMI jest **przygotowane do przeglądu na urządzeniu**;
kompilacja i test sprzętowy v0.7 są jeszcze niepotwierdzone.
Instrukcja oraz lista testów: [docs/HMI_V07.md](docs/HMI_V07.md).
Powrót do znanego źródła v0.6: gałąź `stable/v0.6-hardware-pass`.


Oprogramowanie wyświetlacza informacyjnego InfoTAB na M5Stack Tab5 (ESP32-P4).

## Naprawa: OTA "No response from device" na Windows 11

W `espota.py` ten błąd występuje, gdy po zaproszeniu UDP
InfoTAB nie zestawi przychodzącego TCP do komputera Windows.
Poprawiono uploader: zawsze korzysta z callback TCP 3233
i z adresu komputera wybranego przez trasę sieciową do InfoTAB.

**Jednorazowo** ustaw ograniczoną regułę Windows Defender Firewall
(UAC / uprawnienia administratora):

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup-ota-firewall-windows.ps1
```

Następnie uruchom zwykły uploader:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Skrypt najpierw kompiluje, **potem** pyta o hasło; dopiero
przy prośbie o hasło aktywuj na InfoTAB `OTA WIFI > AKTYWUJ OTA`.
Nie podawaj hasła w rozmowie. Pełna diagnostyka:
[docs/INFOTAB_V06.md](docs/INFOTAB_V06.md).

## InfoTAB v0.6 — aplikacja UPS (Modbus) + Wi-Fi OTA

**Aktualny priorytet:** właściwa aplikacja, brak migracji partycji w tym etapie.

- Odczyt UPS bezpośrednio z `192.168.1.102:502` przez Modbus TCP
  (bateria, obciążenie %, napięcia, szacowany czas; bez NUT).
- Pulpit odświeża tylko zmienne fragmenty — bez migania całego ekranu co 5 s.
- Aktualizacja OTA po Wi-Fi: przycisk `OTA WIFI` na InfoTAB, sesja 5 min,
  jednorazowe hasło, uploader Windows 11.
- Firmware OTA musi mieścić się w **obecnych partycjach 1280 KiB**.
  Nie uruchamiaj `infotab_tab5_16mb_audit -t upload`.

**Jednorazowe wgranie v0.6 przez USB-C (COM6):**

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Dopiero po `BUILD SUCCESS`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

**Kolejne aktualizacje po Wi-Fi** (najpierw `OTA WIFI → AKTYWUJ OTA`
na InfoTAB, potem w terminalu Windows):

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Skrypt zapyta lokalnie o hasło z ekranu; nie podawaj go w rozmowie.
Szczegóły i lista testów: [docs/INFOTAB_V06.md](docs/INFOTAB_V06.md).

**Status:** InfoTAB v0.6 — potwierdzone na urządzeniu: boot po OTA, automatyczne Wi-Fi, Modbus UPS i brak migania (4/4 PASS, 2026-11-06). Kompilacja firmware i transfer OTA również przeszły. Kolejny priorytet: poprawa wyglądu i ergonomii GUI. Produkcyjne podpisy/rollback OTA nadal do wykonania.

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

## InfoTAB v0.5 — pulpit UPS i AI Platform

Zamiast kolejnego etapu migracji OTA rozwijamy działający program.
W wersji v0.5 dodano ekran z rzeczywistymi danymi QNAP NUT
(`192.168.1.101:3493`): zasilanie sieciowe/bateria,
procent naładowania, procent obciążenia UPS i szacunkowy czas
podtrzymania. Widoczny jest też stan AI Bridge (HTTP 200).

W terminalu VS Code na Windows 11:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Po `BUILD SUCCESS` dla środowiska domyślnego `infotab_tab5`
wgrywamy przez COM6:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

Nie uruchamiamy `infotab_tab5_16mb_audit` z `-t upload`.
Pełny opis: [docs/DASHBOARD_V05.md](docs/DASHBOARD_V05.md).
Stan v0.5: **kod na gałęzi roboczej, test Build i test Tab5 do wykonania**.

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
