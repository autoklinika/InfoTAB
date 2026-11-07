# InfoTAB v0.9 — modułowy HMI bez topbara

**Stan:** firmware i dokumentacja przygotowane na gałęzi roboczej.
Kompilacja, OTA i test Tab5 jeszcze **PENDING**.
Poprzednia, sprawdzona wersja zachowana: `stable/v0.8-hmi-pass`.

## Nawigacja i styl

Usunięto górny pasek z czterech ekranów głównych oraz z podstron
Wi-Fi, OTA i diagnostyki. Treść modułu zaczyna się teraz na wysokości
ok. 24 px; wspólna nawigacja nadal znajduje się na dole.
Kolejność, także dla gestu lewo/prawo:

1. **AI PLATFORM**
2. **NAS**
3. **UPS**
4. **USTAWIENIA**

Gest poziomy min. 90 px; zwykły dotyk aktywuje przyciski.
Cykliczne odświeżanie nie czyści całego ekranu: zmienia wyłącznie
obszary aktualnych wartości. Styl v0.8 (jaśniejszy grafit)
pozostaje bez zmian.

## Ekran AI PLATFORM

Dane tylko z `GET http://192.168.1.100:8080/health`:
- stan HTTP / dostępność AI Bridge;
- `version` — rzeczywista wersja interfejsu;
- `components.database` — stan raportowany przez Bridge;
- `components.ollama` — obecnie `not_checked`, czyli
  **NIE SPRAWDZONO**, a nie `ONLINE`.

Cztery duże karty, bez UPS ani statystyk NAS.
GPU, pamięć, modele i RM pozostają poza zakresem tego kroku:
nie udajemy, że /health raportuje takie dane.

## Ekran NAS

Adres QNAP `192.168.1.101`, testy TCP (bez logowania i bez zapisu):
- dostępność NAS na podstawie portów 443 / 8080;
- dostępność panelu QTS, TCP 8080;
- dostępność HTTPS, TCP 443;
- kafelek Dyski/Backup: `BRAK DANYCH API` do czasu
  autoryzowanej integracji z QNAP.

Otwarte porty potwierdzają tylko dostępność usług,
**nie** poprawność RAID, kondycję dysków ani backupów.
Nie korzystamy w tym ekranie z NUT ani z Modbus UPS.

## Ekran UPS

Osiem kafelków bez topbara:
napięcie wejścia, wyjścia, częstotliwość, orientacyjna moc
wyjściowa, naładowanie baterii, obciążenie, pozostały czas,
napięcie baterii. Wszystko z istniejącego Modbus TCP
`192.168.1.102:502` (FC03, tylko odczyt).
Kafelek mocy pozostaje **szacunkowy** do weryfikacji skali.

## Ekran USTAWIENIA

Cztery obszary:
- Wi-Fi: stan i przycisk przejścia do konfiguracji;
- OTA Wi-Fi: stan i przycisk aktualizacji;
- oprogramowanie: wersja i czas pracy InfoTAB;
- diagnostyka: wejście do odczytów partycji i AI Bridge.

Mechanizm OTA z v0.8 bez hasła działa wyłącznie po fizycznym
aktywowania urządzenia i przez maksymalnie 120 s.
W zaufanej sieci LAN nadal istnieje ryzyko przesłania obrazu
przez nieupoważniony host w aktywnym oknie.
Nie zmieniono sposobu zapisu firmware ani tabeli partycji.

## Aktualizacja na Tab5 (Windows 11, bez kabla)

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-powershell-syntax.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Skrypt kompiluje domyślne środowisko `infotab_tab5` i sprawdza
limit dotychczasowego slotu OTA 1280 KiB.
Gdy zapyta o potwierdzenie, w v0.8 na InfoTAB
otwórz `SERWIS → OTA WIFI → AKTYWUJ OTA`, a następnie
naciśnij Enter w Windows.

**Nie wybieraj `infotab_tab5_16mb_audit -t upload`.**

## Odbiór v0.9

- [ ] Build firmware i Wi-Fi OTA: SUCCESS.
- [ ] Brak topbara na czterech ekranach i na podstronach.
- [ ] Dolne zakładki AI Platform / NAS / UPS / Ustawienia.
- [ ] Swipe prawo-lewo przechodzi przez właściwą kolejność.
- [ ] AI Platform pokazuje stan i szczegóły /health.
- [ ] NAS pokazuje realną dostępność, bez zmyślonych danych RAID.
- [ ] UPS ma 8 kafelków z Modbus.
- [ ] Ustawienia prowadzą do Wi-Fi, OTA i diagnostyki.
- [ ] Brak migania przy odświeżaniu.
- [ ] Restart zachowuje Wi-Fi i HMI.
