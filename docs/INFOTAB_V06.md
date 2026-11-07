# InfoTAB v0.6 — pierwszy pulpit użytkowy + Modbus TCP + OTA

Status: kod przygotowany na gałęzi roboczej; **Build na Windows 11 i test na sprzęcie PENDING**.

## Co zmieniono

1. Telemetria **wyłącznie przez Modbus TCP** z UPS `192.168.1.102:502`
   (Unit ID 1, tylko function code 03 Read Holding Registers). **NUT nie
   jest już używany przez aplikację InfoTAB**. Guardian zasilania na
   Minisforum pozostaje bez zmian.
2. Ekran główny z kartami: baterii, obciążenia %, szacowanego
   czasu podtrzymania, napięcia wejścia i wyjścia, AI Bridge i Wi-Fi.
3. Brak pełnego przeładowywania ekranu co 5 sekund. Statyczny pulpit jest
   rysowany tylko przy wejściu do widoku, a odczyty odświeżają wyłącznie
   pola wartości; ekran ustawień sieci nie odświeża się bez potrzeby.
4. OTA Wi-Fi aktywowane **ręcznie przyciskiem `OTA WIFI` na InfoTAB**.
   Losowe hasło (18 znaków) pokazywane tylko na urządzeniu, bez zapisu w
   GitHub i NVS, nasłuch portu 3232 maksymalnie 5 minut.
5. Poprzednia konfiguracja Wi-Fi i przycisk `OTA DIAG` pozostają.

## Rejestry Modbus — potwierdzone na UPS

Mapę skonfrontowano z projektem PowerWalker Modbus v0.1.0 (rejestry
odtworzone z WinPower; inne modele mogą mieć inną mapę), a następnie
odczytano rzeczywiste wartości z naszego UPS przez Minisforum.

| Pole | Adres Modbus zero-based (FC03) | Skala | Wynik przykładowego odczytu 2026-11-06 |
|---|---|---|---|
| Kod trybu pracy | 112, dolny bajt | kod | 4 — normalna praca w aktualnym pomiarze |
| Napięcie wejściowe | 132 | /10 V | 235,0 V |
| Napięcie wyjściowe | 147 | /10 V | 230,0 V |
| Częstotliwość wyjściowa | 144 | /10 Hz | 50,0 Hz |
| Obciążenie wyjścia | 162 | % | 10% |
| Czas minuty | 165 | min | 75 min |
| Czas sekundy | 166 | s | 5 s |
| Bateria | 169 | % | 93% |
| Napięcie baterii | 170 | /10 V | 83,9 V |

NUT służył wyłącznie do jednorazowej weryfikacji stanu online przy
identyfikacji kodu trybu, nie jest źródłem danych pulpitu.

**Moc czynna nie jest jeszcze prezentowana w watach.** Pole 156 zwraca
wartość wymagającą sprawdzenia skali względem wyświetlacza UPS.
Przed publikowaniem mocy [W] należy potwierdzić interpretację.

Źródła mapy:
- https://pypi.org/project/powerwalker-modbus/
- https://powerwalker.com/pl/produkt/10122199/

## Instalacja v0.6 — jednorazowo USB-C

W Windows 11 / VS Code / repo InfoTAB:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Po `BUILD SUCCESS` wgrać **domyślnym środowiskiem 4 MB**:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e infotab_tab5 -t upload
```

To pojedyncza instalacja OTA do aktualnie działającej partycji.
**NIE korzystać z `infotab_tab5_16mb_audit -t upload`**.
Nie dokonujemy zmiany tabeli ani przenoszenia NVS.

## Następne aktualizacje — bez kabla

1. Skompiluj najnowszą wersję projektu, gdy jest gotowa.
2. Na InfoTAB otwórz `OTA WIFI` → `AKTYWUJ OTA`.
3. W VS Code (Windows PowerShell) wykonaj:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Skrypt buduje właściwe `infotab_tab5`, sprawdza limit
**1 310 720 B** bieżącego slotu 1,25 MiB, pyta lokalnie o jednorazowe
hasło widoczne na ekranie i wysyła firmware do `192.168.1.38:3232`.
Urządzenie powinno automatycznie uruchomić nowy obraz.

Jeżeli DHCP przydzieli inny IP, użyj:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1 -DeviceIp 192.168.1.38
```

i podmień IP na adres aktualnie wyświetlany na InfoTAB.

## Diagnostyka OTA na Windows 11 — brak połączenia zwrotnego

W pierwszym teście użytkownik otrzymał w `espota.py`:

```text
[ERROR]: No response from device
Wi-Fi OTA transfer FAILED (exit code 1)
```

Sprawdzono dokładny przepływ `tools/espota.py` Arduino ESP32 3.2.1:
ten błąd pojawia się **po wymianie zaproszenia UDP i ewentualnym
uwierzytelnieniu**, kiedy Windows uruchamia lokalny nasłuch TCP i czeka
na połączenie zwrotne z InfoTAB. Nie oznacza automatycznie problemu
z hasłem, kompilacją czy rozmiarem firmware.

Dotychczasowy uploader używał losowego portu TCP (10000–60000),
który mógł być blokowany przez Windows Defender Firewall.
Dodatkowo pobierał pierwszy interfejs `192.168.1.*`, co nie zawsze
odpowiada trasie do urządzenia w komputerach z VPN/adapterami wirtualnymi.

**Poprawka:** stały callback TCP `3233` oraz automatyczny wybór
adresu lokalnego przez trasę UDP do InfoTAB. Jednorazowo trzeba
utworzyć wąską regułę inbound w zaporze Windows, wyłącznie dla:
`Program = PlatformIO python.exe`, `LocalPort TCP 3233`,
`RemoteAddress = aktualny IP InfoTAB`, `LocalAddress = IP komputera`.

Na Windows 11:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup-ota-firewall-windows.ps1
```

Może pojawić się okno UAC — jest potrzebne tylko do zapisania reguły
zapory. Nie wyłączaj Windows Defender Firewall i nie twórz otwartego
portu dla wszystkich urządzeń.

**Kolejność testu:** po ustawieniu zapory uruchom uploader.
Najpierw kompiluje firmware, a potem prosi o token. Dopiero wtedy
na ekranie `OTA WIFI → AKTYWUJ OTA` włącz sesję na 5 minut
i wpisz wyświetlony token w terminalu. Nie umieszczaj tokenu
w rozmowie ani w repozytorium.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Jeśli IP InfoTAB nie jest już `192.168.1.38`, uruchom **oba skrypty**
z `-DeviceIp` ustawionym na aktualny adres. W razie ponownego błędu
prześlij tylko niesekretne linie z procesu: `Sending invitation`,
`Authenticating`, `Waiting for device` i ostateczny kod błędu.
Nie przełączaj portów routera ani nie wystawiaj OTA do Internetu.

**Status:** pierwszy transfer OTA oraz uruchomienie po aktualizacji PASS (użytkownik 2026-11-06). Wi-Fi, odczyty Modbus i brak migania ekranu — także PASS. Produkcyjne podpisy i automatyczny rollback pozostają odrębne.

## Ograniczenia OTA w tej fazie

- Jest to lokalne **ArduinoOTA do zastosowań deweloperskich**:
  uwierzytelnianie oparte na MD5 i jednorazowym haśle, nie podpis
  kryptograficzny firmware. Używać w zaufanej, izolowanej sieci LAN.
- Token wpisywany do lokalnego procesu `espota.py` może być krótkotrwale
  widoczny dla administratorów systemu Windows przez inspekcję argumentów
  procesów. Nie jest zapisywany w projekcie.
- OTA nie jest uruchomione bez ręcznego aktywowania na urządzeniu.
- Nie wdrożono jeszcze automatycznego rollbacku i podpisów cyfrowych.
- Nie zmienia tabeli 4 MB; OTA działa tylko, gdy firmware mieści się
  w istniejącej partycji 1280 KiB. Migracja 16 MB jest osobnym etapem.
- Jeśli aktualizacja OTA się nie uda, nadal dostępny jest tryb USB-C
  i prywatny backup pełnych 16 MB.

## Test odbiorczy

- [ ] Kompilacja Windows v0.6: BUILD SUCCESS.
- [x] Pulpit uruchamia się i ekran nie miga — PASS (2026-11-06).
- [x] UPS pokazuje pomiary przez Modbus TCP — PASS (2026-11-06).
- [x] Wi-Fi łączy się po aktualizacji — PASS (2026-11-06). Weryfikacja AI Bridge po OTA osobno PENDING.
- [ ] `OTA WIFI` pokazuje jednorazowe hasło i wygasa po 5 minutach.
- [x] Transfer obrazu OTA po Wi-Fi: SUCCESS (użytkownik 2026-11-06).
- [x] Uruchomienie nowej aplikacji po OTA — PASS (2026-11-06).
- [x] Po restarcie OTA wróciły GUI i zapisane Wi-Fi — PASS (2026-11-06).

**Nie wolno traktować tych testów jako PASS, dopóki nie zostaną
potwierdzone na prawdziwym urządzeniu.**
