# InfoTAB v0.9.1 — AI Platform: lista usług (read-only)

Status: kod firmware zapisany, backend statusów uruchomiony i sprawdzony
na Minisforum; bezpośredni dostęp z innych urządzeń LAN **wymaga
jednej wąskiej reguły UFW**. Build/OTA/ekran Tab5: PENDING.

## Widok InfoTAB

Ekran **AI PLATFORM** zawiera wyłącznie listę usług i ich stany.
Jedna linia = nazwa po lewej, stan po prawej, dyskretna kropka statusu.
Bez kafelków, wersji API, wykresów, GPU i pozostałych wskaźników.

Stany:
- **URUCHOMIONA** — systemd ActiveState=active albo Docker Running=true;
- **ZATRZYMANA** — systemd inactive/failed/activating/deactivating albo
  Docker Running=false;
- **BRAK DANYCH** — odczyt niedostępny, niepełny lub nieznany.
  Brak połączenia NIE oznacza, że usługa jest zatrzymana.

13 usług na jednym ekranie:

| Nazwa na InfoTAB | Identyfikator | Mechanizm |
|---|---|---|
| AI Bridge | bridge | systemd systemowy |
| Gateway / RM | gateway | systemd systemowy |
| Ollama | ollama | systemd systemowy |
| Ollama RTX | ollama_rtx | systemd systemowy |
| PostgreSQL | postgres | systemd systemowy |
| Qdrant | qdrant | kontener Docker |
| ComfyUI | comfyui | systemd systemowy |
| RADIO | radio | systemd systemowy |
| UPS Guardian | ups_guardian | systemd systemowy |
| UPS / RM | ups_rm | systemd systemowy |
| Hermes | hermes | systemd użytkownika |
| Magazyn WebGUI | magazyn_web | systemd użytkownika |
| Magazyn Telegram | magazyn_bot | systemd użytkownika |

## Backend na Minisforum

Nowa, oddzielna usługa \`infotab-services-api.service\` uruchomiona
jako zwykły użytkownik \`harrypotter\` (bez roota).
Źródła w repo:
- \`server/infotab_services.py\`
- \`server/infotab-services-api.service\`

Adres: \`http://192.168.1.100:18092/services\`.

GET zwraca **wyłącznie** sztywną listę tekstową:
\`bridge=running\`, \`gateway=running\`, itd.
Nie przyjmuje żadnych nazw usług od klienta, nie wykonuje operacji
start/stop/restart, nie udostępnia poleceń powłoki ani danych logowania.
Dostęp HTTP jest ograniczony kodem do sieci 192.168.1.0/24.
Bez dodatkowej konfiguracji firewall blokuje port 18092 z innych hostów.

Test lokalny:
\`INFOTAB_SERVICES_API_SELFTEST_PASS\`, 13/13 statusów \`running\`.
Systemd użytkownika: active/running; inne usługi produkcyjne
nie były restartowane.

### Jednorazowa reguła UFW — na Minisforum

**Wymaga uprawnień administratora; użytkownik wykonuje lokalnie**.
Zakłada aktualny IP InfoTAB \`192.168.1.38\`. Sprawdź ten adres
na ekranie InfoTAB, gdyż pochodzi z DHCP.
Po upewnieniu się, że adres jest aktualny:

\`\`\`bash
sudo /usr/sbin/ufw allow from 192.168.1.38 to 192.168.1.100 port 18092 proto tcp comment 'InfoTAB readonly services'
\`\`\`

Reguła jest ograniczona do jednego urządzenia, portu i protokołu.
**Nie wyłączać zapory.** Jeśli IP InfoTAB się zmieni,
regułę trzeba zaktualizować; zaleca się rezerwację DHCP.
Nie otwierać portu na routerze WAN.

Następnie można sprawdzić na dowolnym uprawnionym komputerze
(\`192.168.1.38\`) \`GET /services\`.
Po restarcie Minisforum usługa podnosi się automatycznie
przez systemd użytkownika z włączonym linger.

## Firmware Tab5

Odczyt co 8 sekund tylko, gdy otwarty ekran AI Platform.
Pozostałe ekrany NAS, UPS i Ustawienia zachowują dotychczasową
funkcjonalność. Zmiany stanów są nanoszone tylko w prawej kolumnie
danej listy (bez cyklicznego odrysowywania całego ekranu).
Gdy serwer nie odpowiada, wszystkie pozycje przechodzą w
\`BRAK DANYCH\`. Dotychczasowy odczyt Modbus UPS i OTA nietknięte.

### Instalacja firmware przez Wi-Fi

\`\`\`powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-powershell-syntax.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
\`\`\`

Gdy uploader po kompilacji poprosi o Enter, na InfoTAB uruchom
\`USTAWIENIA → OTA WIFI → AKTYWUJ OTA\`. Aktualizacja z v0.8/v0.9
nie wymaga hasła, ale w trakcie 2-minutowego okna OTA jest
otwarta dla innych hostów mogących dosięgnąć urządzenia.
Nie używać środowiska migracyjnego 16MB ani \`erase_flash\`.

## Test odbiorczy

- [x] Backend /services: lokalny odczyt 13/13 usług.
- [x] Backend działa jako niezależna usługa systemd użytkownika.
- [ ] Reguła UFW ograniczona do IP InfoTAB (wymaga sudo lokalnie).
- [ ] GET z InfoTAB przez LAN: HTTP 200 i pełna lista.
- [ ] Build v0.9.1 na Windows: PASS.
- [ ] Wgranie przez Wi-Fi: PASS.
- [ ] Na ekranie AI Platform jest tylko lista 13 usług.
- [ ] Stany i kolory prawidłowe, bez migania.
- [ ] NAS/UPS/Ustawienia oraz gesty nadal działają.
