# InfoTAB — plan wdrożenia

## Etap 0: USB / obraz startowy (pierwszy test zaliczony)
- [x] Struktura projektu PlatformIO dla M5Stack Tab5
- [x] Wskazanie COM6 na Windows 11
- [x] Zweryfikowana lokalnie kompilacja na Windows 11 (`BUILD SUCCESS`, potwierdzone przez użytkownika 2026-11-06)
- [x] Pierwsze wgranie przez COM6 i uruchomienie InfoTAB na ekranie (potwierdzone przez użytkownika 2026-11-06)
- [ ] Potwierdzenie logów portu szeregowego (115200 bps)
- [ ] Potwierdzona rewizja sterownika ekranu na urządzeniu

## Etap 1: ekran i HMI (test dotyku v0.2: PASS)
- [x] Reakcja dotyku na rzeczywistym Tab5 — PASS (potwierdzone przez użytkownika 2026-11-06)
- [ ] Szczegółowa weryfikacja orientacji, narożników i kalibracji dotyku
- [ ] Projekt graficzny ekranu InfoTAB
- [ ] Obsługa błędów, watchdog, komunikaty stanu

## Etap 2: sieć
- [x] Skanowanie Wi-Fi przez ESP32-C6/SDIO — PASS (potwierdzone przez użytkownika 2026-11-06)
- [x] Wykrywanie sieci Wi-Fi na rzeczywistym Tab5 — PASS (potwierdzone przez użytkownika 2026-11-06)
- [x] Kod v0.3: wybór SSID, klawiatura dotykowa i łączenie Wi-Fi (pierwsze połączenie potwierdzone na sprzęcie 2026-11-06)
- [x] Pierwsze połączenie z wybraną siecią Wi-Fi — PASS (potwierdzone przez użytkownika 2026-11-06)
- [ ] Potwierdzenie adresu IP przydzielonego Tab5
- [ ] Automatyczne ponowne połączenie po restarcie, usunięcie danych sieci
- [ ] Test dostępności API AI Platform po Wi-Fi
- [x] Kod konfiguracji sieci bez haseł w repozytorium; zapis po udanym połączeniu (trwałość zapisu wymaga testu restartu)
- [ ] Dostęp do lokalnego API AI Platform / serwera telemetrii UPS
- [ ] Stan połączenia, timeouty, ostatni poprawny odczyt

## Etap 3: telemetria UPS
- [ ] Uzgodnienie kontraktu API z rzeczywistymi danymi UPS
- [ ] Dane: moc, obciążenie, bateria, zasilanie, alarmy (tylko potwierdzone odczyty)
- [ ] Interfejs lokalny UPS, znaczniki czasu, rozłączenia

## Etap 4: OTA
- [ ] Plan partycji aktualizacji dla 16 MB Flash
- [ ] Bezpieczna aktualizacja OTA + sprawdzanie obrazu
- [ ] Potwierdzony rollback / recovery przez USB
- [ ] Wydania firmware i opcjonalna CI w GitHub Actions

## Incydent v0.2 — 2026-11-06

- Użytkownik zgłosił czarny ekran po próbie v0.2, ale v0.1 uruchomił się po naciśnięciu przycisku zasilania.
- Tymczasowo przywrócono działającą wersję v0.1, następnie ponownie przygotowano v0.2 do testu z krótkim naciśnięciem przycisku zasilania po wgraniu.
- Hipoteza: urządzenie pozostawało wyłączone po uploadzie. Przyczyna niepotwierdzona dla v0.2; jeśli problem wystąpi po włączeniu, trzeba odczytać log COM6.
- Ponowny test v0.2 zakończył się sukcesem: użytkownik potwierdził działanie dotyku i skanowania Wi-Fi.
- Czarny ekran najprawdopodobniej wynikał ze stanu zasilania, ale bez zarejestrowanego logu startowego nie uznajemy tej przyczyny za definitywnie potwierdzoną.

Żaden etap niezakończony testem sprzętowym nie jest oznaczany jako PASS.
