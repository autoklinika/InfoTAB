# InfoTAB v0.9.9 — przełącznik ON/OFF i ruchomy napis InfoTAB

Status: **gotowy do testu OTA**, ale jeszcze niezweryfikowany na fizycznym
Tab5. Bazuje na zaakceptowanym przez użytkownika wygaszaczu v0.9.8.
Gałąź: `feature/screensaver-toggle-logo-20261109` (bez merge do `main`).

## Ustawienia

W USTAWIENIA → OPROGRAMOWANIE zamiast przycisku „URUCHOM WYGASZACZ”
jest przełącznik **WYGASZACZ: ON/OFF**.

- **ON** (domyślnie): po 5 minutach bezczynności uruchamia animację;
  pierwsze tapnięcie przywraca poprzedni ekran.
- **OFF**: automatyczne uruchamianie wygaszacza jest zablokowane.
  Wyświetlacz pozostaje włączony i widać zwykły interfejs.
- Zmiana przełącznika jest zapisywana w NVS jako klucz
  `infotab_ui/saver_on` (typ bool) i zachowywana po restarcie
  oraz aktualizacji OTA, która nie czyści NVS.
- Ustawienia Wi-Fi przechowywane są w osobnej przestrzeni NVS
  `infotab_wifi` — przełącznik nie dotyka haseł ani połączenia.
- Przy braku możliwości zapisu NVS stan przełącznika nie zmienia się,
  a zdarzenie błędu pojawia się w logu szeregowym (bez danych poufnych).
- OTA aktywne lub podczas transmisji blokuje przełączenie ustawienia.

## Animacja

Zachowano cztery płynnie przesuwające się pakiety sygnałowe w
technicznych, stonowanych kolorach. Dodano duży, subtelny napis
**InfoTAB** w centralnym, wolnym paśmie ekranu.

Napis przemieszcza się powoli w poziomie i pionie, a jego poprzedni
fragment ekranu jest czyszczony lokalnie. Nie ma stałego nieruchomego
logotypu, pełnoekranowego przerysowywania ani dodatkowych efektów.
LCD nie przechodzi w sleep ani w Display OFF. Jasność podczas
wygaszacza jest obniżana, a potem przywracana do poprzedniego poziomu.

Nie zmieniono backendu i endpointów `/services`, `/nas` i
`/resources`, nawigacji ani obsługi UPS i OTA.

## Test odbiorczy

1. Po OTA sprawdź w Ustawieniach napis `InfoTAB v0.9.9`.
2. Przełącz WYGASZACZ na **OFF**, sprawdź że po 5 minutach wygaszacz
   nie pojawia się.
3. Włącz **ON**, sprawdź czy po 5 minutach uruchamia się animacja
   z **poruszającym się napisem InfoTAB**.
4. Jeden tap powinien przywrócić dokładnie poprzedni ekran,
   bez wywołania przycisku pod palcem.
5. Ustaw **OFF**, zrestartuj urządzenie i sprawdź, czy ustawienie
   nadal pozostaje OFF. Włącz z powrotem ON.
6. Sprawdź, że AI Platform / Zasoby / NAS / UPS i aktualizacja OTA
   działają bez regresji.

## OTA Windows

W PowerShell, w katalogu projektu InfoTAB:

```powershell
git fetch origin
git switch feature/screensaver-toggle-logo-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Na InfoTAB: USTAWIENIA → OTA Wi-Fi → AKTYWUJ OTA, potwierdzenie
w PowerShell. Dotychczasowego `main` nie ruszamy bez osobnej zgody.
