# InfoTAB — odczyty QNAP GlobalNAS (2026-11-08)

## Architektura

- QNAP GlobalNAS: `192.168.1.101` — źródło telemetryczne SNMPv3 / authPriv.
- AI Platform: `192.168.1.100:18092` — istniejące API InfoTAB (tylko LAN `192.168.1.0/24`).
- Tab5: `GET /nas` — prosta lista tekstowa, bez loginów i danych QTS.
- `GET /services` pozostaje bez zmian. Nie dotykamy API UPS, NUT ani Modbus.

## Stan implementacji

Backend wdrożony na AI Platform i zweryfikowany; `/nas` zwraca aktualnie
`nas_state=online`, `snmp_state=not_configured`. Nie ma jeszcze uwierzytelnionych
odczytów SNMP. Firmware: PlatformIO build PASS (1 008 976 B, limit slotu OTA 1 310 720 B).
Wgranie na fizyczny Tab5 i test dotyku: PENDING.

Dane o dyskach, RAID, przestrzeni, wentylatorach oraz backupach domyślnie `--`.
Nie przypisujemy statusu OK urządzeniom lub backupom, których nie zweryfikowaliśmy.
Dotychczasowy odczyt NAS (same porty) zostaje zastąpiony listą 12 pozycji.

## API

`GET http://192.168.1.100:18092/nas` odpowiada `text/plain`; każda linia
ma format `key=value`, stały zestaw kluczy:

```text
nas_state=online
snmp_state=not_configured
cpu_pct=--
ram_pct=--
temp_cpu_c=--
temp_system_c=--
disks=--
raid=--
free_gib=--
fan_rpm=--
uptime_days=--
backup=--
```

Wartości `--` to brak potwierdzonego pomiaru, nie stan dobry ani zły.

## Włączenie SNMPv3 na QNAP

Zalogować się do QTS jako administrator:
Panel sterowania → Sieć i usługi plików → SNMP.

1. Włącz usługę SNMP, port UDP 161.
2. Wybierz SNMP V3, użytkownik `infotab`.
3. Włącz uwierzytelnianie **HMAC-SHA** (8–64 znaków ASCII).
4. Włącz szyfrowanie/prywatność **AES** (osobne hasło, 8–64 znaków ASCII).
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
