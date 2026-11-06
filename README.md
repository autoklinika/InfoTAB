# InfoTAB

Oprogramowanie wyświetlacza informacyjnego InfoTAB na M5Stack Tab5 (ESP32-P4).

## Status

v0.1: kompilacja i uruchomienie przez COM6 potwierdzone. **v0.2: ponowny test na sprzęcie (PENDING)** — poprzednio zgłoszono czarny ekran, ale zaobserwowano, że krótkie naciśnięcie bocznego przycisku włącza urządzenie po wgraniu. Nie uznajemy przyczyny za potwierdzoną do testu.

**Zasada:** gałąź `main` pozostaje stabilna; zmiany z gałęzi roboczych są scalane wyłącznie po zatwierdzeniu.

## Sprzęt

- M5Stack Tab5 (ESP32-P4 + ESP32-C6)
- Komputer programujący: Windows 11, VS Code + PlatformIO
- Pierwsze programowanie: USB-C / COM6
- Późniejszy etap: aktualizacje OTA przez Wi-Fi

## Instrukcja

Przejdź na gałąź `feature/initial-platformio-tab5` i przeczytaj `docs/WINDOWS11.md`.

## Ponowny test diagnostyki v0.2 (Windows 11)

W folderze repozytorium na gałęzi roboczej:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Jeżeli Build kończy się `SUCCESS`, wgraj przez USB-C / COM6:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

Po poprawnym flashu **krótko naciśnij boczny przycisk zasilania, jeśli ekran jest czarny**. Producent: 1 krótkie naciśnięcie = włącz, 2 szybkie = wyłącz; przytrzymanie do migania zielonej diody = tryb programowania. Nie przytrzymuj podczas zwykłego uruchomienia.

Oczekiwany ekran: `InfoTAB v0.2`, test dotyku X/Y i przycisk `SKANUJ WI-FI`. Dotknij ekranu w kilku miejscach, potem naciśnij skanowanie. Żadne hasło nie jest potrzebne; aplikacja nie łączy się z siecią.

Jeżeli po krótkim naciśnięciu ekran nadal jest czarny, zarejestruj log COM6 zamiast ponawiać flash w ciemno. Nie uznajemy v0.2 za sprawdzoną do testu na sprzęcie.

Logowanie przez COM6, 115200 bps (zamknij inne aplikacje zajmujące port):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM6 -b 115200
```

Dokumentacja Wi-Fi i pinów SDIO: https://docs.m5stack.com/en/arduino/m5tab5/wifi
Dotyk: https://docs.m5stack.com/en/arduino/m5unified/touch_class
