# InfoTAB — pierwsze uruchomienie na Windows 11

## Wymagania

- Windows 11, Visual Studio Code
- rozszerzenie PlatformIO IDE
- Git for Windows (lub klonowanie przez VS Code)
- M5Stack Tab5 podłączony przewodem USB-C z transmisją danych
- wykryty port COM6 (jeśli Windows nada inny, zmień `upload_port` i `monitor_port`)

## 1. Pobierz kod

W terminalu Windows PowerShell:

```powershell
git clone https://github.com/autoklinika/InfoTAB.git
cd InfoTAB
git switch feature/initial-platformio-tab5
code .
```

Alternatywnie: VS Code > Source Control > Clone Repository, wklej adres repozytorium,
a następnie przełącz gałąź na `feature/initial-platformio-tab5`.

## 2. Skompiluj

Otwórz folder główny repozytorium w VS Code. PlatformIO wykryje `platformio.ini`.

- PlatformIO: Project Tasks > infotab_tab5 > General > Build
- albo w terminalu z aktywnym PlatformIO: `pio run -e infotab_tab5`

Pierwsza kompilacja pobiera zależności i narzędzia.

## 3. Wgraj firmware

UWAGA: flashowanie zastępuje obecne oprogramowanie Tab5.
W razie potrzeby fabryczne oprogramowanie można przywrócić według instrukcji M5Stack.

- sprawdź port COM6 w Menedżerze urządzeń
- zamknij inne programy używające COM6
- PlatformIO: Upload
- w razie problemu z automatycznym wejściem w download mode użyj procedury BOOT/RESET
  udokumentowanej dla Tab5 przez M5Stack

Po wgraniu na ekranie powinno pojawić się `InfoTAB`, a w monitorze COM6
z prędkością 115200 bps — `[InfoTAB] Boot OK`.

## 4. Kolejne zmiany

```powershell
git switch feature/initial-platformio-tab5
git pull --ff-only
```

Po pobraniu zmian ponownie wybierz Build i Upload.

## Bezpieczeństwo i dalszy plan

- Repozytorium może być publiczne: nie dodawaj kluczy API, haseł, adresów wrażliwych.
- Wi-Fi / OTA będą wdrażane w późniejszym etapie, po konfiguracji partycji i
  mechanizmu odzyskiwania. Sama obecność Wi-Fi nie oznacza jeszcze działania OTA.
- Rewizje ekranów Tab5: ILI9881C+GT911, ST7123 i ST7121.
  Sprawdź oznaczenie na naklejce z tyłu urządzenia, jeśli ekran nie zadziała.
- Nie łącz zmian do `main` bez świadomej zgody właściciela.

Dokumentacja producenta: https://docs.m5stack.com/en/core/Tab5

## Błąd esptool 5.0.0 / Click na Windows 11

Jeżeli na `bootloader.bin` pojawi się:
`TypeError: ParamType.get_metavar() missing 1 required positional argument: 'ctx'`,
to błąd zgodności `esptool 5.0.0` i `click >= 8.2`, a nie firmware.

**Zalecana naprawa:** po `git pull --ff-only` uruchom w PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Skrypt instaluje `click==8.1.8` wyłącznie w prywatnym Pythonie
PlatformIO (`%USERPROFILE%\.platformio\penv`), a następnie uruchamia Build.
Nie zmienia globalnego Pythona w Windows i nie wgrywa firmware do urządzenia.

Nie używamy override `tool-esptoolpy@5.0.2` z ZIP — w połączeniu z
`platform-espressif32#54.03.21` powodował brak pliku `esptool.py`.
Nie ustawiamy `platform_packages`. Platforma `pioarduino/platform-espressif32#54.03.21` sama wskazuje odpowiedni pakiet `pioarduino/esptool` `v5.0.0-dev1`. Próba ręcznego przypięcia `tool-esptoolpy@5.0.0` kończy się `UnknownPackageError`, ponieważ ta specyfikacja nie istnieje w rejestrze.

Jeśli na komputerze pozostał niezgodny pakiet 5.0.2, PlatformIO powinien przywrócić wymagany przez platformę pakiet. Gdyby po `git pull` nadal brakowało `esptool.py`, sprawdź wersję użytego pakietu w wynikach Build — wtedy usuniemy tylko niezgodny pakiet z lokalnego cache PlatformIO i pozwolimy narzędziu pobrać oryginalny.

Po Build sprawdź `SUCCESS`; dopiero wtedy wykonamy Upload na COM6.

Więcej: https://github.com/espressif/esptool/issues/1105
