# InfoTAB v0.8 — test HMI na urządzeniu

**Gałąź do testu:** `feature/hmi-v08-review`. Została utworzona ze znanego
stanu firmware, z zachowanym jednorazowym hasłem OTA (5-minutowe okno).
Nie scalać do `main` przed akceptacją wyglądu.

## Wygląd

Rozjaśniono wszystkie strony, w tym ekrany Wi-Fi, serwisowe i OTA:
- tło slate-blue, RGB565 `0x322A`;
- kafelki jaśniejsze, RGB565 `0x532E`;
- jaśniejsze separatory, wysoki kontrast napisów, stonowane akcenty.
- styl nadal przemysłowy; bez gradientów i dekoracji.

## Ekran UPS

Siatka **4 × 2 kafelki**, wszystkie pomiary w osobnym polu:
- Napięcie wejściowe [V], napięcie wyjściowe [V], częstotliwość [Hz],
  **moc wyjściowa* [W]**;
- Bateria [%], obciążenie [%], czas UPS [min], napięcie baterii [V].

Każdy kafelek ma duży odczyt, tytuł i podpis. Kafelki odświeżają się
indywidualnie, bez migania całego ekranu.

## Moc — wymaga weryfikacji skali

Rejestr Modbus `156` jest klasyfikowany jako moc czynna wyjściowa;
dla naszego UPS jego wartość wynosi obecnie `2`. Rejestr mocy pozornej
`159` zwraca `3` przy 10% obciążenia. Skala dla konkretnego modelu
nie jest oficjalnie potwierdzona. HMI pokazuje **`~200 W`**, zakładając
**hipotezę 0,1 kW na jednostkę rejestru**, i wyraźnie oznacza wynik
`Orientacyjna, reg156`.

To nie jest potwierdzony pomiar energii pobieranej z sieci — odpowiada
przewidywanej mocy **wyjściowej UPS**, bez jego strat. Wartość trzeba
porównać z panelem UPS przy różnych obciążeniach.
Nie przedstawiać tego parametru w raportach jako pomiaru kalibrowanego.

## Aktualizacja do sprawdzenia na Windows 11

W terminalu VS Code:

```powershell
git fetch origin
git switch feature/hmi-v08-review
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Uploader kompiluje wyłącznie środowisko `infotab_tab5` (obecny układ
partycji 1280 KiB); nigdy `infotab_tab5_16mb_audit`.

Po kompilacji, gdy skrypt poprosi o hasło, na InfoTAB:
`SERWIS → OTA WIFI → AKTYWUJ OTA`. Przepisać hasło tylko do
terminala Windows. Nie ujawniać go w rozmowie.

## Odbiór

- [ ] Build i transfer OTA PASS.
- [ ] Strony START, UPS, SIEC/AI, SERWIS czytelne przy normalnym świetle.
- [ ] Każdy z 8 pomiarów UPS w osobnym kafelku.
- [ ] Moc wyjściowa ma prefiks `~` i podpis orientacyjny.
- [ ] Gesty lewo/prawo nadal działają.
- [ ] Modbus, automatyczne Wi-Fi, diagnostyka i OTA działają.
- [ ] Brak migania przy odświeżaniu pomiarów.

**Test sprzętowy v0.8 jeszcze niepotwierdzony.**
