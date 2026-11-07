# InfoTAB v0.5 — pulpit UPS i AI Platform

## Cel

Zastąpienie ekranu testowego pierwszą użyteczną aplikacją na
M5Stack Tab5. **Bez modyfikowania partycji Flash i bez OTA**.

## Dane — faktyczny interfejs

InfoTAB po Wi-Fi pobiera telemetrię z serwera NUT na QNAP NAS:
- host: `192.168.1.101`
- port TCP: `3493`
- nazwa UPS: `qnapups`
- polecenia tylko do odczytu: `GET VAR qnapups ups.status`,
  `battery.charge`, `ups.load`, `battery.runtime`.

Sprawdzono z hosta Minisforum, że QNAP udostępnia powyższe zmienne,
ale odczyt z samego Tab5 wymaga testu sprzętowego (NAS może mieć ACL).

Interpretacja:
- `ups.status` zawiera `OL`: zasilanie sieciowe.
- `OB`: praca z baterii; `LB`: niski poziom baterii;
  `CHRG`: akumulator w trakcie ładowania.
- `battery.charge`: procent baterii (0–100).
- `ups.load`: procent obciążenia względem znamionowej mocy UPS;
  nie jest pomiarem w W.
- `battery.runtime`: szacunkowy czas w sekundach; wyświetlamy minuty
  z wyraźnym oznaczeniem `SZACUNEK UPS`.

Gdy wartości są niekompletne, sprzeczne lub NAS nie odpowiada,
InfoTAB pokazuje **BRAK ODCZYTU NUT** zamiast fikcyjnych danych.

Dodatkowo InfoTAB wykonuje tylko GET
`http://192.168.1.100:8080/health` (AI Bridge).
Stan AI Platform jest niezależny od stanu UPS.

## Układ ekranu

- PULPIT: zasilanie sieciowe / bateria / alarm,
  poziom naładowania, obciążenie %, czas podtrzymania,
  AI Bridge ONLINE/OFFLINE i aktualny IP InfoTAB.
- `ODSWIEZ`: ręczne odświeżenie telemetrii.
- `SIEC WIFI`: istniejąca konfiguracja z ekranu, zmiana sieci,
  zapomnienie zapisanych danych.
- `OTA DIAG`: dotychczasowy ekran tylko do odczytu.

Odświeżanie automatyczne co 8 sekund. Widok nie steruje
UPS ani nie wykonuje komend NUT zmieniających stan urządzenia.

## Test sprzętowy Windows 11

Na gałęzi `feature/initial-platformio-tab5` w terminalu VS Code:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Po `BUILD SUCCESS` (środowisko domyślne: stary układ 4 MB) wgrać:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

**Nie używać środowiska `infotab_tab5_16mb_audit` do Upload.**

Po wgraniu, w razie czarnego ekranu krótko naciśnij boczny
przycisk zasilania.

## Wynik odbiorczy — do potwierdzenia

- [ ] Kompilacja v0.5 na Windows 11: SUCCESS.
- [ ] Pulpit jest widoczny, bez czarnego ekranu.
- [ ] Wi-Fi łączy się z zapamiętaną siecią bez pytania o hasło.
- [ ] UPS pokazuje rzeczywisty status, naładowanie i obciążenie.
- [ ] Przycisk ODSWIEZ odświeża wskazania.
- [ ] AI Bridge pokazuje ONLINE przy HTTP 200.
- [ ] Powrót do ustawień Wi-Fi oraz diagnostyki działa.
- [ ] Zachowano poprawny powrót po restarcie.

Jeżeli NAS odrzuca żądania z InfoTAB (np. ACL klientów NUT),
nie zmieniaj polityki ochrony QNAP w ciemno. Wtedy
przygotujemy prostą bramkę tylko do odczytu po stronie Minisforum.

## Ograniczenia aktualnej wersji

- Obciążenie wyświetlane w procentach, bez wartości W,
  ponieważ QNAP NUT nie publikuje `ups.realpower`.
- Brak mechanizmu zapisu firmware OTA. Pozostaje odłożony.
- Brak gwarancji łączności z NAS z samego ESP32-P4 do testu.
- Rozmiar firmware musi pozostać w obecnym slocie 1280 KiB.
