# InfoTAB v0.9.7 — DISPLAY OFF + wybudzanie tapnięciem (wersja testowa)

## Dlaczego zmieniliśmy metodę

Użytkownik przetestował v0.9.6: panel przechodzi w tryb Sleep In,
ale **nie reaguje na tapnięcie**. To zgodne z ryzykiem występującym na
Tab5 z wyświetlaczem i dotykiem ST7121/ST7123 (zintegrowany TDDI).
Nie twierdzimy jeszcze, że dokładnie ten wariant powoduje awarię:
potrzebny jest test na rzeczywistym sprzęcie.

Zamiast komendy **SLPIN (0x10)**, wysyłamy do panelu komendę MIPI DCS
**DISPOFF (0x28)**, a przy wybudzeniu **DISPON (0x29)**. Obie komendy
przekazywane są przez `Panel_DSI::getBusDSI()->writeParams`; generyczne
`M5.Display.writeCommand()` nie zapewnia wysyłania przez DSI.
Następnie osobno wyłączamy i przywracamy podświetlenie, zachowując
ustawiony poziom jasności.

**To nie jest wyłączenie zasilania LCD.** Panel pozostaje zasilany,
a jego kontroler nie otrzymuje komendy SLPIN. Obraz jest wyłączony,
czyli nie pokazuje statycznej treści. Celem jest dalsze skanowanie
dotyku. Na danym egzemplarzu trzeba sprawdzić, czy kontroler TDDI
faktycznie działa z wyłączonym wyświetlaniem.

## Działanie HMI

- USTAWIENIA → OPROGRAMOWANIE → **WYLACZ EKRAN (TEST)**
  natychmiast wysyła DISPLAY OFF po puszczeniu dotyku.
- Pierwszy tap, o ile jest odbierany, przywraca DISPLAY ON i jasność;
  pierwszy tap nie może uruchomić przycisku HMI.
- Jeśli tap nie zostanie wykryty, podczas pierwszego testu ekran wraca
  samoczynnie po 20 sekundach, aby uniknąć trwałego czarnego obrazu.
- Dopiero potwierdzenie `DOTYK OK` wyłącza testowy automatyczny powrót.
- Na głównych zakładkach automatyczne wyłączanie po 5 minutach bez
  dotyku; pierwszy automatyczny test ma fallback po 60 sekundach.
- Zdarzenie `AUTO / BRAK DOTYKU` oznacza, że rezygnujemy z kolejnych
  automatycznych prób do czasu udanego ręcznego testu/restartu.
- Zabezpieczone są OTA i konfiguracja Wi-Fi; podczas transferu
  firmware automatyczne wyłączenie obrazu jest zablokowane.
- Backend InfoTab/AI Platform/NAS/UPS nie podlega zmianom.

## Próby po OTA

1. Sprawdź w USTAWIENIACH numer v0.9.7.
2. Naciśnij **WYLACZ EKRAN (TEST)** — ekran powinien zgasnąć.
3. W ciągu około 5 sekund tapnij ekran — powinien wrócić na ten sam
   widok, pokazując `DOTYK OK`. Pierwszy tap nie otwiera zakładki.
4. Jeśli tap nie działa, nie naciskaj przycisków zasilania: ekran
   powinien wrócić automatycznie po maks. 20 sekundach i pokazać
   `AUTO / BRAK DOTYKU`.
5. Jeśli tap zadziała, przejdź do ZASOBY i sprawdź automatyczne
   wyłączenie po 5 minutach oraz ponowne wybudzenie.
6. Sprawdź także API i nawigację — szczególnie wejście do OTA
   po ponownym wybudzeniu.

## Ograniczenia

Bez fizycznego testu nie można stwierdzić, że DISPLAY OFF zachowa
dotyk na każdej rewizji Tab5. Jeśli ST712x wyłącza skanowanie dotyku
również w DISPLAY OFF, konieczne będzie rozwiązanie alternatywne
(np. wybudzenie osobnym przyciskiem sprzętowym albo utrzymanie
sterownika w aktywnym stanie i wyłączenie jedynie wyświetlania bez DCS
DISPOFF). Nie deklarować produkcyjnego PASS przed testem.

Repo: gałąź `feature/panel-display-off-tap-wake-20261109`.
**Produkcyjny `main` pozostaje bez zmian.**

## Windows / OTA

```powershell
git fetch origin
git switch feature/panel-display-off-tap-wake-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Uaktywnić Wi-Fi OTA bez hasła fizycznie na InfoTab w USTAWIENIACH,
a następnie potwierdzić Enter w PowerShell. Nie zmieniamy partycji.
