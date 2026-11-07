# InfoTAB v0.3 — konfiguracja Wi-Fi na ekranie

## Zakres

Użytkownik nie musi podawać SSID ani hasła na komputerze. Interfejs Tab5 umożliwia:
1. Dotknięcie przycisku `WYBIERZ SIEC`.
2. Skanowanie dostępnych sieci przez ESP32-C6 (SDIO).
3. Wybór SSID na ekranie (do 32 wyników, po 7 na stronę).
4. Wpisanie hasła na ekranowej klawiaturze QWERTY (`SHIFT`, `SYM`, `SPACJA`, `KASUJ`).
5. Połączenie i odczyt adresu IP oraz RSSI.
6. Automatyczne łączenie z zapamiętaną siecią po restarcie.
7. Ponowne połączenie lub usunięcie zapamiętanej sieci.

Program nie zawiera haseł. Nie wypisuje SSID ani haseł w Serial Monitor.
Hasło jest maskowane na ekranie. Dla zabezpieczonej sieci wymagane jest 8–63
znaków **ASCII**; sieci otwarte nie wymagają hasła.
Ukryte SSID nie są jeszcze obsługiwane. Program nie wprowadza statycznego IP.

## Instalacja Windows 11

Na gałęzi `feature/initial-platformio-tab5`, w folderze projektu:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Po `[InfoTAB] BUILD SUCCESS` (dopiero wtedy):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

Jeżeli ekran pozostaje czarny po flashowaniu, krótko naciśnij boczny
przycisk zasilania. Nie przytrzymuj go. Urządzenie może pozostać
wyłączone po trybie programowania.

## Test odbiorczy na urządzeniu

- [ ] Panel główny InfoTAB v0.3 jest widoczny.
- [ ] `WYBIERZ SIEC` wykrywa SSID i RSSI.
- [ ] Wybór sieci zabezpieczonej otwiera klawiaturę.
- [ ] Wpisywane znaki są maskowane; usuwanie i SHIFT/SYM działają.
- [ ] Po poprawnym haśle pojawia się `POLACZONO`, lokalny IP i RSSI.
- [ ] Po restarcie InfoTAB sam łączy się z zapisaną siecią.
- [ ] Błędne hasło powoduje timeout, bez nadpisywania dotychczasowych danych.
- [ ] `ZAPOMNIJ SIEC` usuwa ustawienia i wyłącza autołączenie.

## Bezpieczeństwo

Dane sieci są zapisywane w `Preferences` (NVS na urządzeniu) **dopiero po
udanym połączeniu**. Domyślny NVS bez szyfrowania pamięci Flash **nie zapewnia
ochrony przed fizycznym odczytem**. To konfiguracja testowa lokalnego
urządzenia; do zastosowań wymagających silnej ochrony hasła potrzebne jest
szyfrowanie NVS/Flash lub odrębna sieć IoT z odpowiednią izolacją.

Nie umieszczaj haseł, tokenów ani plików z sekretami w GitHubie.
Nie drukuj haseł w logach. Na tym etapie nie uruchamiamy OTA ani połączenia z UPS.

## Diagnostyka

Jeśli nie łączy się z siecią, sprawdź pasmo 2,4 GHz i ustawienia WPA2/WPA3
w UniFi. Sieci typu Enterprise/802.1X i ukryte SSID nie są objęte v0.3.
Po 20 sekundach bez połączenia ekran wraca do ekranu głównego.

Log USB, wyłącznie gdy potrzebny:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM6 -b 115200
```

Źródła:
- Wi-Fi/SDIO Tab5: https://docs.m5stack.com/en/arduino/m5tab5/wifi
- Touch M5Unified: https://docs.m5stack.com/en/arduino/m5unified/touch_class
- NVS Preferences: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html
