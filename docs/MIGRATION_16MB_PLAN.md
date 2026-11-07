# InfoTAB — plan migracji tabeli 4 MB → 16 MB (DRAFT / NO FLASH)

Stan na 2026-11-06. **Nie wykonujemy migracji ani OTA w tej wersji procedury.**
Plan powstał po sprzętowym preflight z Windows 11 i COM6.

## Dowody potwierdzone

- ESP32-P4 rev. 1.3, **Flash fizyczny 16 MB** (identyfikacja ROM/esptool).
- Aktualna tablica: NVS 0x9000/0x5000; otadata 0xE000/0x2000;
  app0 0x10000/0x140000, app1 0x150000/0x140000;
  SPIFFS 0x290000/0x160000; coredump 0x3F0000/0x10000.
- Kandydat 16 MB: app0 0x10000/0x640000,
  app1 0x650000/0x640000, SPIFFS 0xC90000/0x360000;
  coredump 0xFF0000/0x10000.
- Kandydat `infotab_tab5_16mb_audit` Build PASS:
  firmware 932416 B, po 5 621 184 B zapasu w każdym slocie.
- Zrzut **całych 16 MiB** Flash do prywatnego katalogu Windows
  zweryfikowany SHA-256 i zgodnością dotychczasowych partycji.
  Zrzut zawiera potencjalnie hasło Wi-Fi; **nie publikować**.
- Efuse/security: **Secure Boot disabled, Flash Encryption disabled**,
  `SPI_BOOT_CRYPT_CNT=0`. Informacja diagnostyczna,
  nie jest automatycznym zezwoleniem na modyfikację firmware.
- Audyt offline starego obrazu pokazał dane inne niż `0xFF` w
  `app1`, `spiffs` i `coredump`. Nie odczytywano ich zawartości.

## Co się nakłada

Nowe `app0` zajmuje zakres [0x10000, 0x650000).
Obejmuje to stare `app1` [0x150000, 0x290000),
stare `spiffs` [0x290000, 0x3F0000)
i stare `coredump` [0x3F0000, 0x400000).

Nawet jeśli stary SPIFFS nie jest używany przez bieżący kod InfoTAB,
jego dane nie mogą być uznane za zbędne jedynie na podstawie braku
odwołań `SPIFFS` w `src/main.cpp`. Potrzebna decyzja o zachowaniu /
migracji / odrzuceniu danych **po identyfikacji ich znaczenia**.

NVS i otadata zachowują *te same adresy* w obu tabelach. To umożliwia
zaplanowanie migracji bez ich nadpisywania, ale nie zastępuje backupu
i testu, że nowy bootloader i firmware poprawnie je odczytują.
Uwaga: otadata może kierować uruchomienie do nieoczekiwanego slotu.

## Warunki wejścia w rzeczywistą migrację USB

1. Utrzymać pełny, prywatny backup 16 MiB (i najlepiej dodatkową
   zaszyfrowaną kopię offline); potwierdzić sprawność dysku.
2. Potwierdzić, że ekran, dotyk i połączenie Wi-Fi nadal działają po
   wykonaniu backupu/preflight.
3. Ustalić znaczenie danych w starym SPIFFS i coredump:
   niczego nie kasować i nie udostępniać surowej zawartości w rozmowie.
4. Sprawdzić aktywną konfigurację bootloadera, jego offset i zgodność
   z nową tabelą partycji; zweryfikować czy otadata wskaże poprawne app0.
5. Przygotować dokładny plan obszarów pamięci przewidzianych do zapisu
   (adres, rozmiar, SHA-256 dla każdego binarnego artefaktu).
6. Opracować lokalną **procedurę przywrócenia** starej tabeli, starego
   obrazu aplikacji i NVS z prywatnej kopii w razie nieudanego startu.
   Nie proponować automatycznego pełnego `erase_flash`.
7. Przetestować procedurę odczytu i zawartość artefaktów *offline*,
   bez żadnych operacji zmieniających urządzenie.
8. Dopiero po oddzielnym zatwierdzeniu przez właściciela projektu
   przygotować operatorowi instrukcję rzeczywistej migracji przez USB.
9. Po migracji sprawdzić: boot, GUI, dotyk, SSID/IP, AI Bridge,
   odczyt partycji 6,25 MiB, odtwarzanie po restarcie.
10. Pierwsze OTA wykonać dopiero osobnym etapem po sprawdzeniu
    zachowania rollbacku oraz autentyczności firmware.

## Polityka bezpieczeństwa i zależności

Nie wykonywać `pio run -e infotab_tab5_16mb_audit -t upload`.
Nie używać `esptool write-flash` ani `erase-flash` przed domknięciem
warunków i osobnym zatwierdzeniem planu.
Nie rozszerzać istniejącego API Bridge o endpoint aktualizacji
bez oddzielnego projektu autoryzacji i integralności obrazu.
Nie włączać Secure Boot/Flash Encryption podczas tej migracji —
to odrębna decyzja projektowa z własnym planem odzyskania.

Odniesienia:
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/system/ota.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/partition-tables.html
- https://github.com/espressif/arduino-esp32/blob/3.2.1/tools/partitions/default_16MB.csv

**Stan: READY FOR MIGRATION DESIGN REVIEW — NOT READY FOR FLASH.**
