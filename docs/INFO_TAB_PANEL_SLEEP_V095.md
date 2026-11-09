# InfoTAB v0.9.6 — diagnostyka LCD Sleep In i wybudzenie dotykiem

Stan: **wersja diagnostyczna, firmware v0.9.6 nie został jeszcze wgrany na Tab5**.
Wersja v0.9.5 została zgłoszona jako **nieusypiająca panelu**, nie możemy
zdalnie potwierdzić, czy przyczyną jest licznik czy działanie panelu MIPI.

Zmiany opracowano na gałęzi `feature/panel-sleep-diagnostics-20261109`,
bez łączenia do produkcyjnego `main`.

## Cel i sprzęt

Tab5 ma ekran 5" IPS TFT LCD, **nie OLED**. Typowa cecha LCD to
możliwość chwilowego utrwalania obrazu przy bardzo długiej ekspozycji,
ale nie jest to identyczne ze zjawiskiem trwałego wypalenia OLED.

Nie stosujemy tylko czarnego obrazu ani `setBrightness(0)`.
M5GFX `M5.Display.sleep()` wykonuje dwie operacje:

1. Gasi podświetlenie ekranu.
2. Wysyła przez MIPI-DSI do sterownika LCD komendę `SLPIN` (0x10),
   przełączając **sam kontroler LCD w sleep**.

To **nie jest fizyczne odłączenie zasilania modułu LCD**, które
mogłoby odciąć również zintegrowany kontroler dotyku ST7121/ST7123.
Unikamy także `M5.Power.deepSleep`, bo zatrzymałby obsługę dotyku
i wymagał innego sposobu wybudzenia.

## Działanie

- Panel automatycznie zasypia po **5 minutach** bez dotyku, tylko na
  głównych modułach AI PLATFORM / ZASOBY / NAS / UPS / USTAWIENIA.
  Odliczanie nie zależy od dostępności Wi-Fi, aby ekran nie pozostał
  włączony podczas awarii łączności.
- Ekrany konfiguracji sieci, diagnostyki i OTA nie przechodzą w
  automatyczny sleep; OTA w toku również blokuje uśpienie.
- ESP32-P4 i Wi-Fi pozostają włączone; `M5.update()` nadal
  odczytuje dotyk.
- Pierwszy tap wybudza LCD (`M5.Display.wakeup()`, MIPI `SLPOUT`),
  nie aktywując przycisków pod palcem.
- Po wybudzeniu pełne odrysowanie aktywnego widoku i
  nowe pobranie wartości z istniejącego API.
- Logika czasu oparta na odejmowaniu `millis()`, bez osobnego timera.
- Nie modyfikujemy endpointów serwera, UPS, NAS, RM ani parametrów GPU.

### Zabezpieczenie podczas sprawdzania sprzętu

Tab5 istnieje z różnymi panelami (ILI9881C + GT911, ST7123 i ST7121).
Nie zakładamy, że zintegrowany kontroler ST7123 zawsze raportuje
dotyk po komendzie LCD SLPIN. Dopóki nie potwierdzimy choć jednego
wybudzenia **rzeczywistym dotykiem**, firmware awaryjnie budzi ekran
po **60 sekundach** od automatycznego uśpienia
(a przy ręcznym teście po **20 sekundach**). Gdy pierwszy tap zadziała,
zabezpieczenie jest wyłączane do następnego restartu;
odtąd panel pozostaje w stanie sleep aż do tapnięcia.
Zabezpieczenie nie jest mechanizmem cyklicznego wzbudzania w stanie
produkcyjnym.

## Diagnostyka v0.9.6 (od razu, bez czekania 5 minut)

Nowy przycisk w **USTAWIENIA → OPROGRAMOWANIE → USPIJ EKRAN (TEST)**:
- fizyczne puszczenie przycisku wywołuje natychmiast `M5.Display.sleep()`;
- bez tapnięcia ekran sam wróci po **20 sekundach**, dopóki nie
  potwierdzono działania wybudzenia dotykiem;
- po wybudzeniu przy `Idle Ns` zostaje status: `DOTYK OK`
  albo `AUTO / BRAK DOTYKU`;
- po automatycznym fallbacku wyłączamy kolejne próby automatycznego
  usypiania, dopóki nie przetestujemy ręcznego wybudzenia albo
  nie zrestartujemy InfoTab, żeby urządzenie nie migało co 5 minut;
- po udanym wybudzeniu dotykiem automatyczne uśpienie po 5 minutach
  pozostaje aktywne, bez fallbacku co minutę.

Pole `Idle Ns` w USTAWIENIACH pokazuje, czy nieznane sygnały dotyku
resetują licznik. Jeśli stale pokazuje 0–kilkanaście sekund mimo braku
dotykania, należy zająć się filtrowaniem sygnałów dotyku, a nie
obsługą MIPI LCD.

`main` produkcyjny nie jest zmieniony; nie instalujemy nowych usług
na Minisforum, NAS lub UPS.

## Odbiór fizyczny po OTA

1. Po OTA sprawdź w USTAWIENIACH numer **v0.9.6**.
2. Naciśnij `USPIJ EKRAN (TEST)` — LCD ma od razu zasnąć.
3. Jeśli zgasł, dotknij go raz; ma wrócić bez restartu.
   Jeśli dotyk nie działa, po maks. 20 sekundach ekran powinien wrócić sam.
4. Zwróć uwagę, czy status pokazuje `DOTYK OK` lub
   `AUTO / BRAK DOTYKU`. Opisz też, czy LCD faktycznie zgasł.
5. Gdy dotykowe wybudzenie się udało, zostaw InfoTab na ZASOBY
   bez dotykania przez 5 minut.
6. Tapnięcie wybudzające nie powinno zmienić zakładki ani uruchomić OTA.
7. Przejdź do USTAWIENIA / OTA i upewnij się, że aktywna procedura
   OTA nie jest przerywana przez uśpienie.
8. Zweryfikuj odczyty NAS, UPS, CPU/RAM/Radeon/RTX po wybudzeniu.

**Ryzyko sprzętowe:** na niektórych rewizjach ST7121/ST7123 komenda
sleep może wpłynąć na zintegrowany dotyk. Fallback po 60 sekundach
ogranicza ryzyko nieodwracalnie czarnego ekranu w pierwszych testach,
ale dotykowe wybudzenie pozostaje do zweryfikowania.

## Aktualizacja przez Wi-Fi (bez merge do main)

W PowerShell w katalogu InfoTAB:

```powershell
git fetch origin
git switch feature/panel-sleep-diagnostics-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Na urządzeniu: `USTAWIENIA → OTA Wi-Fi → AKTYWUJ OTA`,
a następnie zatwierdzenie w PowerShell.
