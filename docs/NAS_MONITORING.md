# InfoTAB — odczyty QNAP GlobalNAS (2026-11-09)

## Architektura

- QNAP GlobalNAS: `192.168.1.101` — źródło telemetryczne SNMPv3 / authPriv.
- AI Platform: `192.168.1.100:18092` — istniejące API InfoTAB (tylko LAN `192.168.1.0/24`).
- Tab5: `GET /nas` — prosta lista tekstowa, bez loginów i danych QTS.
- `GET /services` pozostaje bez zmian. Nie dotykamy API UPS, NUT ani Modbus.

## Stan implementacji

Backend wdrożony na AI Platform i zweryfikowany. SNMPv3 HMAC-SHA + CBC-DES
autoryzowany; potwierdzono rzeczywiste pomiary CPU, RAM, temperatur, czterech
dysków `ready`, dwóch wolumenów `Ready`, 2 wentylatorów i wolnej przestrzeni.
Firmware: PlatformIO BUILD PASS po rozszerzeniu do 13 wierszy (1 009 168 B, slot OTA 1 310 720 B).
Użytkownik potwierdził fizyczny test OTA i pozostałych odczytów 2026-11-09.
Do wskaźników RAID i backup NIE jest konieczne ponowne OTA; wystarczy backend.

RAID: QNAP zwraca dwie grupy, obie `Ready` z
`1.3.6.1.4.1.24681.1.4.1.1.1.2.1.2.1.5.*` (2/2 READY).
Nie mylić tego z kondycją SMART lub wolumenów (osobne odczyty).

Backup: źródło to lokalne raporty Stage K monitorujące kopie AI Platform
na GlobalNAS (daily.json, weekly.json, monitor.json). Status OK wymaga
ostatniego monitoringu PASS do 3h, dziennego PASS do 36h, tygodniowego
PASS do 8 dni, bez `issues`. Jawne FAIL oznacza ALARM, brak świeżości
NIEAKTUALNY, brak danych `--`. To NIE jest monitor QNAP HBS 3. Nie
montujemy NAS ani nie uruchamiamy zadań backupu przez API InfoTab.

## API

`GET http://192.168.1.100:18092/nas` odpowiada `text/plain`; każda linia
ma format `key=value`, stały zestaw kluczy:

```text
nas_state=online
snmp_state=ok
cpu_pct=1
ram_pct=29
temp_cpu_c=31
temp_system_c=20
disks=4/4 GOTOWE
raid=2/2 READY
volumes=2/2 READY
free_gib=7490
fan_rpm=548/547
uptime_days=2
backup=AI STAGE K: OK
```

Wartości `--` to brak potwierdzonego pomiaru, nie stan dobry ani zły.

## Włączenie SNMPv3 na QNAP

Zalogować się do QTS jako administrator:
Panel sterowania → Sieć i usługi plików → SNMP.

1. Włącz usługę SNMP, port UDP 161.
2. Wybierz SNMP V3, użytkownik `infotab`.
3. Włącz uwierzytelnianie **HMAC-SHA** (8–64 znaków ASCII).
4. Włącz szyfrowanie/prywatność **CBC-DES** (zgodnie z ograniczeniem aktualnego panelu QTS; osobne hasło, 8–64 znaków ASCII). DES jest przestarzały kryptograficznie; SNMP ograniczamy wyłącznie do lokalnej sieci LAN. Jeśli NAS udostępnia AES, preferujemy AES.
5. Zapisz. Jeżeli dostępna jest lista dozwolonych źródeł, dopuść
   `192.168.1.100` (Minisforum); **nie przekierowuj UDP 161 na Internet**.
6. Opcjonalnie pobierz `NAS.mib` z QTS do późniejszych prac. Używane
   OID zostały już potwierdzone empirycznym odczytem na tym NAS.

Na Minisforum wyłącznie w terminalu SSH (nie w czacie i nie przez repo GitHub):

```bash
python3 /home/harrypotter/.local/share/infotab-services-api/setup_qnap_snmp.py
```

Hasła są ukryte podczas wpisywania i zapisane do
`~/.config/infotab/nas-snmp.json` z uprawnieniami 0600.
Backend korzysta z `pysnmp` 7.1.32 w prywatnym venv.
W skrypcie wybierz `DES` (wartość domyślna) przy panelu QTS z `CBC-DES`; wybór `AES` jest przeznaczony dla QNAP, na którym faktycznie udostępniono AES. Uwierzytelnianie pozostaje HMAC-SHA.
Hasła nie są częścią firmware, odpowiedzi HTTP ani repo.

Kolektor używa potwierdzonych na urządzeniu OID `NAS-MIB`:
`1.3.6.1.4.1.24681.1.3.*` (system, dyski, wolumeny, wentylatory)
oraz `1.3.6.1.4.1.24681.1.4.1.1.1.2.1.*` (grupy RAID).
`sysUpTime.0` odzwierciedla uptime agenta SNMP, natomiast na
ekranie wyświetlane jest uptime NAS z `systemUptimeEX`.

## Wdrożenie firmware

Repo: `autoklinika/InfoTAB`, gałąź robocza, **bez merge do main**.
Build i wgranie Tab5 potwierdzono w bieżącej rozmowie 2026-11-09.
Dalsze rozszerzenia ograniczone do backendu nie wymagają OTA.
Brak automatycznego OTA z samej AI Platform.

## Testy i rollback

Przed zmianą production:
`GET /services` zapisane i porównane byte-for-byte po zmianie.
Wdrożono wyłącznie użytkownikową usługę `infotab-services-api.service`;
nie zmieniono usług UPS/backup/RM.

Backup kodu i unit:
`~/.local/share/infotab-services-api/backups/20261108-nas-api/`.
Do rollbacku przywrócić oba pliki i zrestartować wyłącznie
`systemctl --user restart infotab-services-api.service`.

## Zweryfikowane OID i skale (QNAP 2026-11-08)

- systemInfoEX `1.3.6.1.4.1.24681.1.3.1..6.0`: CPU, RAM, uptime, temperatury.
- Dyski `1.3.6.1.4.1.24681.1.3.11.1.4.*`: stan 0 = ready, -5 = no disk; nie jest to niezależny test SMART.
- Wolumeny `1.3.6.1.4.1.24681.1.3.17.1.5.*`, `...1.6.*`: wolna przestrzeń i status. Zmierzona skala liczby odpowiada KiB, pomimo opisu 'byte' w starym MIB; zestawiono z legacy formatem `273.62 GB` i `7.05 TB`.
- Wentylatory `1.3.6.1.4.1.24681.1.3.15.1.3.*`: RPM.
- RAID: `1.3.6.1.4.1.24681.1.4.1.1.1.2.1.1.0` (liczba grup) oraz
  `1.3.6.1.4.1.24681.1.4.1.1.1.2.1.2.1.5.*` (stan grup). 2 grupy `Ready` 2026-11-09.
- Backup AI Platform: zapisane statusy Stage K
  `/srv/ai-data/platform/backup-status/stage-k/`; ostatnie daily PASS 2026-11-09,
  weekly PASS 2026-11-08 oraz monitor PASS 2026-11-09.
  Status HBS 3 pozostaje poza zakresem; `AI STAGE K: OK` nie oznacza stanu kopii HBS.
