# InfoTAB — plan wdrożenia

## Etap 0: USB / obraz startowy (pierwszy test zaliczony)
- [x] Struktura projektu PlatformIO dla M5Stack Tab5
- [x] Wskazanie COM6 na Windows 11
- [x] Zweryfikowana lokalnie kompilacja na Windows 11 (`BUILD SUCCESS`, potwierdzone przez użytkownika 2026-11-06)
- [x] Pierwsze wgranie przez COM6 i uruchomienie InfoTAB na ekranie (potwierdzone przez użytkownika 2026-11-06)
- [ ] Potwierdzenie logów portu szeregowego (115200 bps)
- [ ] Potwierdzona rewizja sterownika ekranu na urządzeniu

## Etap 1: ekran i HMI
- [ ] Test dotyku i orientacji
- [ ] Projekt graficzny ekranu InfoTAB
- [ ] Obsługa błędów, watchdog, komunikaty stanu

## Etap 2: sieć
- [ ] Łączność Wi-Fi przez koprocesor ESP32-C6
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

Żaden etap niezakończony testem sprzętowym nie jest oznaczany jako PASS.
