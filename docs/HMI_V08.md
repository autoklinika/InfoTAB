# InfoTAB v0.8 — jaśniejsze HMI i komplet kafelków UPS

Status: kod na gałęzi roboczej, test kompilacji oraz przegląd na urządzeniu **PENDING**.

## Zmiany wizualne

- Tło wszystkich ekranów rozjaśniono do przygaszonego koloru slate-blue
  (około #354656, RGB565 0x322A).
- Karty są wyraźnie jaśniejsze (około #506475, RGB565 0x532E).
- Jasne typografie i separatory; zielony i bursztynowy nadal oznaczają
  stan, nie pełnią funkcji dekoracyjnej.
- Zachowane cztery ekrany START / UPS / SIEC-AI / SERWIS,
  pasek zakładek i gest poziomy.
- Ustawienia Wi-Fi, serwis i OTA mają ten sam jaśniejszy kolor tła.
- Odświeżanie nie wywołuje `fillScreen()` co 5 sekund.

## Ekran UPS — każdy pomiar w osobnym kafelku

Siatka 4 kolumny × 2 rzędy na ekranie 1280×720:

| Rząd 1 | Rząd 1 | Rząd 1 | Rząd 1 |
| --- | --- | --- | --- |
| Napięcie wejściowe V | Napięcie wyjściowe V | Częstotliwość Hz | Moc wyjściowa* W |

| Rząd 2 | Rząd 2 | Rząd 2 | Rząd 2 |
| --- | --- | --- | --- |
| Bateria % | Obciążenie % | Szacowany czas min | Napięcie baterii V |

Każdy kafelek ma własną nazwę, duży odczyt i krótki podpis
techniczny. Pozostałe ekrany pozostają oparte na istniejącym HMI.

## Moc — ważna uwaga o dokładności

Użytkownik poprosił o pobór mocy. Na ten moment Modbus TCP dostarcza
dwie wartości: rejestr 156 = `2` (moc czynna wyjściowa), rejestr 159 =
`3` (moc pozorna wyjściowa), przy obciążeniu 10% i UPS 3 kVA.

Biblioteka PowerWalker Modbus identyfikuje te rejestry, ale nie potwierdza
skali dla naszego modelu. Hipoteza (wymaga porównania z panelem UPS):
`moc_wyjsciowa = rejestr_156 × 100 W`, czyli orientacyjnie `~200 W`.
**Kafelek celowo pokazuje znak `~` i podpis `Orientacyjna, reg156`.**
Nie jest to gwarantowany pomiar w watach ani całkowity pobór UPS
z sieci z uwzględnieniem strat i ładowania baterii.

Należy porównać wynik z panelem UPS przy co najmniej dwóch
różnych obciążeniach. Bez tego nie uznajemy watów za zweryfikowane.

## Test na fizycznym InfoTAB

W Windows 11 / VS Code:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Gdy uploader poprosi o token, na urządzeniu wybierz
`SERWIS → OTA WIFI → AKTYWUJ OTA`.
Używamy sprawdzonej, pięciominutowej sesji z jednorazowym hasłem.
Nie zmieniamy partycji Flash ani NVS.

Po restarcie:
- [ ] Cztery strony i gest lewo/prawo.
- [ ] UPS: osiem równych kafelków, żadna wartość nie wystaje.
- [ ] Jaśniejszy wygląd jest czytelny na stanowisku.
- [ ] Moc ma prefiks `~`; brak odczytu to `--`.
- [ ] Modbus odświeża dane; brak migania.
- [ ] Wi-Fi i OTA pozostają dostępne.

**Status:** walidacja struktury kodu PASS, test Build i działanie na sprzęcie PENDING.
