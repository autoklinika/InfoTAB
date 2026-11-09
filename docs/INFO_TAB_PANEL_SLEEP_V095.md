# InfoTAB v0.9.5 — uśpienie panelu LCD i wybudzenie dotykiem

Stan: **wersja testowa, firmware nie został jeszcze wgrany na Tab5**.
Zmiany opracowano na gałęzi `feature/panel-sleep-tap-wake-20261109`,
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
po **60 sekundach** od zaśnięcia. Gdy pierwszy tap zadziała,
zabezpieczenie jest wyłączane do następnego restartu;
odtąd panel pozostaje w stanie sleep aż do tapnięcia.
Zabezpieczenie nie jest mechanizmem cyklicznego wzbudzania w stanie
produkcyjnym.

## Odbiór fizyczny po OTA

1. Pozostaw InfoTab na ZASOBY bez dotykania przez 5 minut. Panel powinien
   całkowicie zgasnąć (LCD sleep, nie tylko czarne tło).
2. Dotknij ekranu raz. Widok ma wrócić bez restartu urządzenia.
3. Tapnięcie wybudzające nie powinno zmienić zakładki ani uruchomić OTA.
4. Ponownie odczekaj 5 minut, ale tym razem nie dotykaj ekranu przez
   przynajmniej minutę. Po prawidłowym pierwszym wybudzeniu panel
   nie powinien sam wrócić.
5. Przejdź do USTAWIENIA / OTA i upewnij się, że aktywna procedura
   OTA nie jest przerywana przez uśpienie.
6. Zweryfikuj odczyty NAS, UPS, CPU/RAM/Radeon/RTX po wybudzeniu.

**Ryzyko sprzętowe:** na niektórych rewizjach ST7121/ST7123 komenda
sleep może wpłynąć na zintegrowany dotyk. Fallback po 60 sekundach
ogranicza ryzyko nieodwracalnie czarnego ekranu w pierwszych testach,
ale dotykowe wybudzenie pozostaje do zweryfikowania.

## Aktualizacja przez Wi-Fi (bez merge do main)

W PowerShell w katalogu InfoTAB:

```powershell
git fetch origin
git switch feature/panel-sleep-tap-wake-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Na urządzeniu: `USTAWIENIA → OTA Wi-Fi → AKTYWUJ OTA`,
a następnie zatwierdzenie w PowerShell.
