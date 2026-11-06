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
- [x] Test dostępności AI Bridge `/health` bezpośrednio z Tab5 po Wi-Fi: HTTP 200 (zdjęcie użytkownika, 2026-11-06)
- [x] Kod konfiguracji sieci bez haseł w repozytorium; trwałość zapisu potwierdzona testem restartu 2026-11-06
- [ ] Dostęp do lokalnego API AI Platform / serwera telemetrii UPS
- [ ] Stan połączenia, timeouty, ostatni poprawny odczyt

## Etap 3B: InfoTAB v0.7 — wieloekranowy HMI

- [x] Dodano 4 ekrany główne: START / UPS / SIEC-AI / SERWIS.
- [x] Dodano gest poziomy oraz dolne zakładki, oddzielono tap i swipe.
- [x] Spójna typografia, grafitowa paleta, stonowany kolor akcentowy.
- [x] Odświeżanie tylko obszarów wartości — bez cyklicznego `fillScreen()`.
- [x] Zostawiono sprawdzony Modbus TCP i OTA przez Wi-Fi.
- [x] Zabezpieczono kod v0.6 w `stable/v0.6-hardware-pass`.
- [ ] Build na Windows 11 i transfer OTA v0.7: PENDING.
- [ ] Potwierdzenie wyglądu na rzeczywistym ekranie: PENDING.
- [ ] Test swipe, zakładek, dotyku, aktualizacji danych: PENDING.

## Etap 3A: v0.6 — docelowy transport i GUI

- [x] Odczyt rzeczywistego PowerWalker przez Modbus TCP `192.168.1.102:502` z Minisforum (FC03, rejestry 112/128/144/165).
- [x] Zmapowano i potwierdzono baterię 93%, obciążenie 10%, napięcie 230 V, pozostały czas 75:05 w odczycie testowym.
- [x] Zastąpiono w firmware źródło NUT bezpośrednim Modbus TCP.
- [x] Usunięto pełne `render()` wykonywane co 5 s; pulpit odświeża tylko zmienne obszary.
- [x] Dodano nowe rozmieszczenie kart, statusów i przycisków.
- [x] Dodano ręczny 5-minutowy odbiornik OTA z losowym hasłem na ekranie oraz uploader Windows.
- [ ] Windows Build v0.6 i kontrola, czy binarka mieści się w 1280 KiB.
- [x] InfoTAB v0.6 uruchamia się na sprzęcie, odczytuje UPS przez Modbus i nie miga ekranem — PASS (potwierdzenie użytkownika 2026-11-06).
- [x] Transfer OTA przez Wi-Fi: `WIFI OTA TRANSFER SUCCESS` (potwierdzenie użytkownika 2026-11-06). Autoryzacja/połączenie TCP i przesłanie obrazu zakończone sukcesem.
- [x] Po Wi-Fi OTA: poprawny boot, automatyczne Wi-Fi, rzeczywisty Modbus i brak migania LCD — 4/4 PASS (potwierdzenie użytkownika 2026-11-06).
- [ ] Produkcyjne uwierzytelnianie/podpisy i rollback: późniejszy etap.

## Etap 3: telemetria UPS — główny priorytet

- [x] Potwierdzono dostępne pola z QNAP NUT (z Minisforum): `ups.status`, `battery.charge`, `ups.load`, `battery.runtime`.
- [x] Dodano do InfoTAB v0.5 pulpit UPS + AI Bridge, pobierający odczyty po Wi-Fi; stan sprzętowy PENDING.
- [ ] Build v0.5 na Windows 11 i Upload przez COM6.
- [ ] Potwierdzić bezpośredni odczyt NUT z Tab5; jeśli NAS blokuje dostęp, stworzyć proxy read-only na Minisforum.
- [ ] Potwierdzić alarm OB/LB, odświeżanie i zachowanie przy zaniku sieci.

## Etap 3: telemetria UPS
- [ ] Uzgodnienie kontraktu API z rzeczywistymi danymi UPS
- [ ] Dane: moc, obciążenie, bateria, zasilanie, alarmy (tylko potwierdzone odczyty)
- [ ] Interfejs lokalny UPS, znaczniki czasu, rozłączenia

## Etap 4: OTA
- [x] Ustalono konflikt: board manifest sugeruje `default_16MB.csv` (app0/app1 po 6,25 MiB), ale **urządzenie faktycznie używa** app0/app1 po **1280 KiB** (`default.csv` / layout 4 MB). OTA BLOCKED do rozwiązania.
- [x] Diagnostyka v0.4 na sprzęcie PASS: app0 0x10000/1280 KiB, app1 0x150000/1280 KiB, boot=running, Wi-Fi online, AI Bridge HTTP 200 (zdjęcia 2026-11-06)
- [x] Dodano i uruchomiono read-only `scripts/audit-partitions.py` (standardowy build i wariant 16 MB: oba PASS 2026-11-06)
- [x] Rozmiar działającego obrazu odczytany z urządzenia: 932384 B (zdjęcia 2026-11-06)
- [x] Lokalny audyt obecnego `partitions.bin` wykonany; 4 MB layout zgodny ze zdjęciami (użytkownik 2026-11-06)
- [x] Przygotowano osobny wariant kompilacji `infotab_tab5_16mb_audit` z jawnie wskazanym CSV 16 MB; BUILD PASS (2026-11-06, 02:03)
- [x] Potwierdzono lokalnie dwa sloty OTA po 6400 KiB, firmware 932416 B, wolne 5621184 B w każdym (2026-11-06)
- [x] Fizyczna pamięć Flash potwierdzona przez COM6: 16 MB, ESP32-P4 rev. 1.3; `esptool flash-id` READ ONLY PASS (2026-11-06)
- [x] Skrypt backupu pełnych 16 MiB i walidacja rozmiaru, SHA-256 oraz starego layoutu: PASS na rzeczywistym sprzęcie (2026-11-06)
- [x] Potwierdzono pełny, lokalny backup 16 MiB: `BACKUP VERIFIED - READ ONLY PASS`, SHA-256, stary układ partycji (2026-11-06). Kopia prywatna, nie publikować.
- [x] Read-only audyt prywatnej kopii i eFuse/ROM: PASS techniczny (2026-11-06), wykryto NON-ERASED w starych app1/spiffs/coredump
- [x] Secure Boot: Disabled, Flash Encryption: Disabled, SPI_BOOT_CRYPT_CNT=0, flags=0 — potwierdzone na ESP32-P4 (2026-11-06)
- [ ] Ustalić, czy dane w starym SPIFFS są użytkowe; nie zakładać, że NON-ERASED oznacza dane do odrzucenia
- [x] Przygotowano `docs/MIGRATION_16MB_PLAN.md` — plan migracji z warunkami wejścia i zasadą bez zapisów (DRAFT)
- [ ] Sprawdzić stan bootloadera i przygotować plan bezpiecznej migracji USB bez pełnego kasowania Flash
- [ ] Plan migracji partycji do pełnej przestrzeni 16 MB: backup/odzyskanie NVS, USB recovery, test rollback
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
