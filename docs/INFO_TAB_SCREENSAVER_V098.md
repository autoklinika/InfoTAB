# InfoTAB v0.9.8 — animowany wygaszacz LCD

**Wersja testowa.** Użytkownik odrzucił usypianie i wyłączanie panelu,
ponieważ nie udało się niezawodnie wzbudzić go dotykiem. To rozwiązanie
jest alternatywą niewymagającą stanów LCD Sleep In / Display Off.
Gałąź: `feature/animated-screensaver-20261109`; **bez merge do main**.

## Styl i działanie

- Ekran bezczynny przez **5 minut** przechodzi w animowany wygaszacz.
- Pełny ekran, niemal czarne grafitowe tło, **cztery powoli poruszające
  się impulsy telemetryczne** w stonowanych kolorach. Żadnych statycznych
  napisów, dat, przycisków, jasnych podświetleń i efektownych animacji.
- Odświeżanie ~22 klatki/s (45 ms); animacja **nie przerysowuje**
  całego ekranu w każdej klatce, lecz usuwa poprzednie niewielkie
  obszary impulsów i rysuje je ponownie.
- Podświetlenie jest ustawiane na około 55% normalnego poziomu
  podczas wygaszacza, ale **panel nie zasypia i nie jest wyłączany**.
  Przy powrocie przywracana jest pierwotna jasność.
- Pierwsze dotknięcie wyłącza wygaszacz, bez wykonywania akcji HMI.
  Dotychczasowa zakładka wraca z nowym odczytem danych.
- Wygaszacz nie działa w podstronach konfiguracji Wi-Fi, OTA lub
  diagnostyki OTA. Aktywne/uzbrojone OTA wyklucza jego uruchomienie.
- Telemetria wybranego modułu nadal jest pobierana w tle, co 15 s,
  ale nie jest rysowana na ekranie wygaszacza.

## Test natychmiastowy

W USTAWIENIA → OPROGRAMOWANIE dodano przycisk
**URUCHOM WYGASZACZ**. Pozwala sprawdzić animację bez czekania
pięciu minut. Pierwszy tap powinien przywrócić Ustawienia.
Wcześniejsze funkcje Wi-Fi i OTA pozostają takie same.

## Ograniczenia

Wygaszacz zmniejsza czas wyświetlania nieruchomych elementów i pobór
podświetlenia, ale nie odłącza zasilania wyświetlacza. Jest to ekran
IPS LCD, nie OLED. Zachowanie animacji i dotyku należy zatwierdzić na
rzeczywistym Tab5; kompilacja i testy statyczne **nie zastępują
testu fizycznego**.

## OTA z Windows

W PowerShell, w katalogu InfoTAB:

```powershell
git fetch origin
git switch feature/animated-screensaver-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Na fizycznym urządzeniu aktywuj USTAWIENIA → OTA Wi-Fi →
AKTYWUJ OTA i potwierdź aktualizację w PowerShell.

## Kryteria odbioru

1. Wersja `InfoTAB v0.9.8` w Ustawieniach.
2. Kliknięcie URUCHOM WYGASZACZ wyświetla płynną animację i lekko
   obniża jasność, ale nie wyłącza LCD.
3. Pierwszy tap wraca do Ustawień, bez otwierania innej opcji.
4. Po 5 minutach na ZASOBY wygaszacz uruchamia się sam.
5. Tap przywraca ZASOBY wraz z pomiarami CPU/RAM/Radeon/RTX.
6. NAS/RAID/backup, UPS i lista usług AI działają jak wcześniej.
7. Aktywne OTA nie jest przerywane przez wygaszacz.
