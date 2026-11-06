# InfoTAB — lokalna kopia pełnej pamięci Flash przed migracją OTA

**Status:** narzędzia gotowe, backup rzeczywistego urządzenia jeszcze niepotwierdzony.  
**Data:** 2026-11-06.  
**Cel:** zachować odtwarzalny obraz całych 16 MiB Flash, w tym NVS z konfiguracją Wi-Fi.

## Potwierdzone parametry

- ESP32-P4 rev 1.3, pamięć Flash: **16 MB**, `esptool flash-id`: PASS.
- Obecny firmware: app0 0x10000 / 1280 KiB, app1 0x150000 / 1280 KiB.
- Nowy wariant `infotab_tab5_16mb_audit`: BUILD + audyt PASS;
  app0 i app1 mają po 6400 KiB, bez wgrywania na Tab5.
- Aktualna konfiguracja sieci działa. **Jeszcze nic nie migrujemy.**

## Backup na Windows 11 (TYLKO ODCZYT)

1. Podłącz InfoTAB przez USB-C (COM6), zamknij Serial Monitor.
2. Otwórz terminal PowerShell w folderze repozytorium InfoTAB.
3. Uruchom:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\backup-flash-windows.ps1
```

Skrypt uruchamia wyłącznie:

```text
esptool --chip esp32p4 --port COM6 read-flash 0x0 0x1000000 <plik-tymczasowy>
```

Następnie `verify-flash-backup.py` sprawdza dokładny rozmiar **16 777 216 B**,
SHA-256 i zgodność starego układu partycji `nvs`, `otadata`, `app0`, `app1`.
Skrypt nie wypisuje zawartości NVS ani haseł.

Kopia jest zapisywana w:

```text
%LOCALAPPDATA%\InfoTAB\FlashBackups\
```

To katalog użytkownika Windows, **poza repozytorium GitHub**.
Skrypt nie wykonuje `erase-flash`, `write-flash` ani `upload` i nie
wgrywa partycji 16 MB. Może zresetować ESP32 po odczycie;
jeśli ekran jest czarny, użyj krótkiego naciśnięcia przycisku bocznego.

## Oczekiwany wynik

```text
[InfoTAB] Full flash backup VERIFIED
  Image length: 16777216 bytes (16 MiB)
  Original partitions: NVS/otadata/app0/app1 match running 4MB layout.
[InfoTAB] BACKUP VERIFIED - READ ONLY PASS.
[InfoTAB] Flash has NOT been modified.
```

**Do rozmowy wolno wkleić jedynie tekstowy wynik testu, nie plik .bin.**
Pełny dump zawiera potencjalnie jawne dane uwierzytelniające Wi-Fi.

## Zasady bezpieczeństwa

- Włącz BitLocker / Device Encryption na dysku z backupem.
- Nie umieszczaj kopii w GitHubie, chmurze publicznej ani załącznikach czatu.
- Nie zapisuj kopii w `C:\PROJEKTY\InfoTAB` ani w katalogach synchronizowanych.
- Na potrzeby migracji zachowaj ten plik wraz z SHA-256.
- Nie wykonuj `-t upload` dla `infotab_tab5_16mb_audit` przed zakończeniem
  procedury migracji i potwierdzeniem ścieżki odzyskiwania.

## Warunki dla migracji partycji

Po potwierdzonym backupie nadal trzeba:
1. Zweryfikować uruchomienie Tab5 i poprawność Wi-Fi po odczycie.
2. Przeanalizować bootloader, `otadata`, stan zabezpieczeń i zakresy
   nadpisywane podczas zmiany tabeli partycji.
3. Przygotować atomową, kontrolowaną procedurę USB-C bez `erase_flash`,
   odrębną od standardowego `PlatformIO Upload`.
4. Zaplanować powrót do starego układu z lokalnej kopii oraz test
   rollbacku OTA. **Nie wykonujemy automatycznego przywrócenia 16 MB
   dumpa bez sprawdzenia zgodności docelowej płytki.**

Obecny status migracji: **BLOCKED — backup do potwierdzenia**.
