# InfoTAB — odczyty QNAP GlobalNAS (2026-11-08)

## Architektura

- QNAP GlobalNAS: `192.168.1.101` — źródło telemetryczne SNMPv3 / authPriv.
- AI Platform: `192.168.1.100:18092` — istniejące API InfoTAB (tylko LAN `192.168.1.0/24`).
- Tab5: `GET /nas` — prosta lista tekstowa, bez loginów i danych QTS.
- `GET /services` pozostaje bez zmian. Nie dotykamy API UPS, NUT ani Modbus.

## Stan implementacji

Backend wdrożony na AI Platform i zweryfikowany. SNMPv3 HMAC-SHA + CBC-DES
autoryzowany; potwierdzono rzeczywiste pomiary CPU, RAM, temperatur, czterech
dysków `ready`, dwóch wolumenów `Ready`, 2 wentylatorów i wolnej przestrzeni.
Firmware: PlatformIO BUILD PASS po rozszerzeniu do 13 wierszy (1 009 168 B, slot OTA 1 310 720 B); wgranie
na fizyczny Tab5 i test dotyku: PENDING.

`raid` i `backup` pozostają `--` do niezależnego potwierdzenia ich
rzeczywistych stanów. `Ready` wolumenów nie stanowi potwierdzenia RAID
Healthy ani poprawnego SMART. Brak portu oznacza offline, brak metryki oznacza `--`.

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
raid=--
volumes=2/2 READY
free_gib=7490
fan_rpm=548/547
uptime_days=2
backup=--
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
6. Pobierz `NAS.mib` z QTS — jest niezbędny do zweryfikowania OID w danej
   wersji NAS i dokończenia sekcji RAID/dyski/wolna przestrzeń.

Na Minisforum wyłącznie w terminalu SSH (nie w czacie i nie przez repo GitHub):

```bash
python3 /home/harrypotter/.local/share/infotab-services-api/setup_qnap_snmp.py
```

Hasła są ukryte podczas wpisywania i zapisane do
`~/.config/infotab/nas-snmp.json` z uprawnieniami 0600.
Backend korzysta z `pysnmp` 7.1.32 w prywatnym venv.
W skrypcie wybierz `DES` (wartość domyślna) przy panelu QTS z `CBC-DES`; wybór `AES` jest przeznaczony dla QNAP, na którym faktycznie udostępniono AES. Uwierzytelnianie pozostaje HMAC-SHA.
Hasła nie są częścią firmware, odpowiedzi HTTP ani repo.

W tej chwili collector używa tylko potwierdzonych w starszym
`NAS-MIB` OID systemInfoEx `1.3.6.1.4.1.24681.1.3.*` oraz
`sysUpTime.0` (uptime **agenta**, tylko weryfikacja SNMP).
QNAP w nowszych wersjach może używać innego MIB. Aby w pełni zmapować
dyski, RAID, wolumeny, wentylatory i temperatury — najpierw trzeba
zweryfikować odczyty `NAS.mib` pobranego z tego konkretnego NAS.

## Wdrożenie firmware

Repo: `autoklinika/InfoTAB`, gałąź robocza, **bez merge do main**.
Należy przeprowadzić build, test na M5Stack Tab5 i wgranie Wi-Fi OTA
zgodnie z istniejącą procedurą fizycznego aktywowania okna OTA.
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
- `raid`, `backup` — brak autorytatywnych odczytów, pozostają nieznane.
