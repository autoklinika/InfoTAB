# InfoTAB — plan wdrożenia

## Etap 0: USB / obraz startowy (pierwszy test zaliczony)
- [x] Struktura projektu PlatformIO dla M5Stack Tab5
- [x] Wskazanie COM6 na Windows 11
- [x] Zweryfikowana lokalnie kompilacja na Windows 11 (`BUILD SUCCESS`, potwierdzone przez użytkownika 2026-11-06)
- [x] Pierwsze wgranie przez COM6 i uruchomienie InfoTAB na ekranie (potwierdzone przez użytkownika 2026-11-06)
- [ ] Potwierdzenie logów portu szeregowego (115200 bps)
- [ ] Potwierdzona rewizja sterownika ekranu na urządzeniu

## Etap 1: ekran i HMI (v0.2 wstrzymana — czarny ekran po wgraniu)
- [ ] Test dotyku i orientacji (próba v0.2: czarny ekran, wymagane rozdzielenie testów)
- [ ] Projekt graficzny ekranu InfoTAB
- [ ] Obsługa błędów, watchdog, komunikaty stanu

## Etap 2: sieć
- [ ] Test Wi-Fi przez ESP32-C6/SDIO (próba v0.2: czarny ekran, test niepotwierdzony)
- [ ] Potwierdzenie widoczności sieci 2,4 GHz na rzeczywistym Tab5
- [ ] Konfiguracja sieci bez haseł w repozytorium
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

- Użytkownik zgłosił czarny ekran po próbie v0.2.
- Kod przywrócono na gałęzi roboczej do **dokładnie tej samej wersji v0.1**, która działała na urządzeniu.
- Przyczyna nieustalona (brak logu bootowania z COM6); nie zgadujemy, czy winna jest inicjalizacja Wi-Fi, LCD czy proces uruchamiania.
- Następny krok: potwierdzić powrót v0.1, następnie osobno testować dotyk i Wi-Fi.

Żaden etap niezakończony testem sprzętowym nie jest oznaczany jako PASS.
