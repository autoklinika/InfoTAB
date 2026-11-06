# InfoTAB

Oprogramowanie wyświetlacza informacyjnego InfoTAB na M5Stack Tab5 (ESP32-P4).

## Status

v0.1: pierwsza kompilacja i uruchomienie przez COM6 potwierdzone na Windows 11. **Po czarnym ekranie przy próbie v0.2 przywrócono dokładny działający firmware v0.1 na gałęzi roboczej.** Testy dotyku i Wi-Fi będą osobnymi etapami.

**Zasada:** gałąź `main` pozostaje stabilna; zmiany z gałęzi roboczych są scalane wyłącznie po zatwierdzeniu.

## Sprzęt

- M5Stack Tab5 (ESP32-P4 + ESP32-C6)
- Komputer programujący: Windows 11, VS Code + PlatformIO
- Pierwsze programowanie: USB-C / COM6
- Późniejszy etap: aktualizacje OTA przez Wi-Fi

## Instrukcja

Przejdź na gałąź `feature/initial-platformio-tab5` i przeczytaj `docs/WINDOWS11.md`.

## Odzyskiwanie firmware v0.1 (Windows 11)

W folderze repozytorium na gałęzi roboczej:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Jeżeli Build kończy się `SUCCESS`, wgraj przez USB-C / COM6:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

Na ekranie powinien ponownie pojawić się napis `InfoTAB` oraz linia `USB firmware: uruchomiony`. Jest to powrót do działającego obrazu v0.1; dotyk i Wi-Fi nie są jeszcze uruchamiane.

Logowanie przez COM6, 115200 bps (zamknij inne aplikacje zajmujące port):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM6 -b 115200
```

Dokumentacja Wi-Fi i pinów SDIO: https://docs.m5stack.com/en/arduino/m5tab5/wifi
Dotyk: https://docs.m5stack.com/en/arduino/m5unified/touch_class
