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
- [x] Adres IP przydzielony przez DHCP, odczytany na Tab5: 192.168.1.38 (potwierdzone przez użytkownika 2026-11-06)
- [x] Automatyczne ponowne połączenie po restarcie — PASS (potwierdzone przez użytkownika 2026-11-06)
- [ ] Usunięcie danych sieci (funkcja dostępna, test sprzętowy PENDING)
- [x] Endpoint AI Bridge `/health` z hosta 192.168.1.100:8080 odpowiada HTTP 200 (test na Minisforum)
- [ ] Test dostępności API AI Platform `/health` bezpośrednio z Tab5 po Wi-Fi
- [x] Kod konfiguracji sieci bez haseł w repozytorium; trwałość zapisu potwierdzona testem restartu 2026-11-06
- [ ] Dostęp do lokalnego API AI Platform / serwera telemetrii UPS
- [ ] Stan połączenia, timeouty, ostatni poprawny odczyt

## Etap 3: telemetria UPS
- [ ] Uzgodnienie kontraktu API z rzeczywistymi danymi UPS
- [ ] Dane: moc, obciążenie, bateria, zasilanie, alarmy (tylko potwierdzone odczyty)
- [ ] Interfejs lokalny UPS, znaczniki czasu, rozłączenia

## Etap 4: OTA
- [x] Weryfikacja domyślnej tabeli partycji dla ESP32-P4 / Arduino 3.2.1: NVS + otadata + app0/app1 po 0x640000 (6.25 MiB), bez konieczności zmiany partycji
- [x] Dodany na ekranie odczyt aktywnej i zapasowej partycji OTA, bez zmian Flash (v0.4-preflight; test sprzętowy PENDING)
- [ ] Potwierdzenie rozmiaru obrazu .bin i odczytu aktywnej partycji OTA z urządzenia
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
