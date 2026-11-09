# InfoTAB ZASOBY — pomiary sprzętu Minisforum (v0.9.4, 09.11.2026)

## Źródło i architektura

Przesłany przez użytkownika raport „Odczyt parametrów Minisforum” potwierdza
dostęp do informacji CPU, RAM, Radeon 890M oraz RTX 5060 Ti. Raport proponował
rozbudowę AI Bridge; istniejące API InfoTAB jest już jednak działającą,
ograniczoną do LAN usługą odczytową, dlatego używamy go bez zmian AI Bridge.

* Host: Minisforum, Ubuntu, serwis użytkownika `infotab-services-api.service`.
* GET `http://192.168.1.100:18092/resources`: 19 stałych kluczy `key=value`
  (ASCII, text/plain, tylko sieć 192.168.1.0/24).
* Odświeżanie kolektora co 3,5 sekundy (cache), InfoTab co 4 sekundy;
  na LCD przepisywane są tylko zmienione liczby.
* Czujniki: `/proc/stat`, `/proc/meminfo`, `/proc/uptime`,
  `/sys/class/hwmon`, `/sys/class/drm`, skalujące `cpufreq`;
  RTX: wyłącznie `nvidia-smi --query-gpu=...` z timeoutem 2,2 sekundy,
  z wyborem GeForce RTX 5060 Ti według nazwy, niezależnie od kolejności GPU.
* Brak instalacji na NAS; brak SNMP, żadnych kluczy w urządzeniu.
  Brak dostępnego pomiaru = `--`, nigdy domyślne 0.

## Ekran

Zachowujemy pięć zakładek AI PLATFORM / ZASOBY / NAS / UPS / USTAWIENIA.

ZASOBY: 3 pionowe sekcje (bez kafelków):
* **CPU / RAM:** temperatura CPU, obciążenie, średnie bieżące taktowanie,
  RAM zajęty/całkowity (GiB), temperatury dwóch NVMe, uptime hosta;
* **RADEON 890M:** temperatura, obciążenie, raportowana pamięć użyta / 96 GiB,
  moc. Pamięć ta jest współdzielona (unified memory), **nie stanowi
  dodatkowych 96 GiB niezależnej pamięci fizycznej**;
* **RTX 5060 Ti:** temperatura, obciążenie, VRAM (MiB, dla precyzji
  przy minimalnym użyciu), moc, wentylator (%; 0% może oznaczać fan stop).

Nie wyświetlamy wartości historycznych z PDF jako „na żywo”.
Obciążenie CPU jest 160-ms delta z /proc/stat; pamięć systemowa to
MemTotal - MemAvailable. Taktowanie to średnia z dostępnych rdzeni.

## Bezpieczeństwo i odporność

Bez komend modyfikujących system, bez zmian Resource Manager, zadań GPU
lub adaptera eGPU. Odczyt nvidia-smi ma ograniczenie czasu i działa bez
dodatkowego obciążenia obliczeniowego GPU. Stale 19 nazw pól, maks.
niewielka odpowiedź tekstowa, odmowa żądań spoza LAN. W przypadku błędu
czujnika `--`; w razie całkowitej utraty API ekran pokazuje `--`.

`/services` i `/nas` pozostają na dotychczasowych ścieżkach.
Nie zmieniamy firmware OTA ani rozkładu partycji przed odbiorem.

## Wdrożenie na Tab5

Gałąź robocza `feature/resources-telemetry-20261109`; nie łącz z main
bez zgody. W PowerShell w katalogu InfoTAB:

```powershell
git fetch origin
git switch feature/resources-telemetry-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Na InfoTAB należy wybrać USTAWIENIA > OTA Wi-Fi > AKTYWUJ OTA, a następnie
zatwierdzić w PowerShell. Wgranie/oględziny na rzeczywistym urządzeniu PENDING.

## Wynik testu hosta (przykładowy, chwilowy)

CPU ~31°C, RAM ~7,3/30,5 GiB; Radeon 29°C, ~21,9/96 GiB, ~4,7 W;
RTX 35°C, 2/16 311 MiB, ~3,5 W; NVMe ~35°C i ~26°C.
Odczyty zmieniają się na bieżąco i nie stanowią stałego stanu sprzętu.
