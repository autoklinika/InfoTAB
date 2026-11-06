# InfoTAB v0.8 — jaśniejsze HMI i OTA bez wpisywania hasła

Status: kod przygotowany na gałęzi roboczej. Kompilacja i test sprzętowy **PENDING**.

## Zmiany w HMI

- Rozjaśniono tło oraz kafelki w grafitowej palecie.
- Na ekranie UPS **wszystkie osiem parametrów ma własny kafelek**:
  napięcie wejścia i wyjścia, częstotliwość, moc wyjściowa
  (orientacyjna), poziom baterii, obciążenie, czas podtrzymania,
  napięcie baterii.
- Gesty lewo/prawo oraz dolne zakładki z v0.7 bez zmian.
- Odświeżanie wyłącznie pól pomiarowych — bez cyklicznego czyszczenia LCD.

**Moc:** dla naszego Phoenixtec Modbus register 156 dotychczas zwracał
wartość 2, a reg159 wartość 3 przy 10% obciążeniu UPS 3 kVA.
Wyświetlamy *orientacyjnie* odpowiednio ok. 200 W i 300 VA,
zakładając skalę 0,1 kW / 0,1 kVA (niepotwierdzoną jeszcze
porównaniem z wyświetlaczem UPS). Kafelek jest oznaczony
`MOC WYJ. *` i `Orientacyjna, reg156`.
**Nie jest to pomiar mocy wejściowej UPS pobieranej z sieci.**
Przy braku danych pokazujemy `--`, nigdy wartości zastępcze.

## OTA bez ręcznie wpisywanego hasła

Od v0.8 odbiornik OTA nie żąda hasła. Jego uruchomienie wymaga
**fizycznego dotknięcia InfoTAB** na ekranie Serwis → OTA Wi-Fi →
Aktywuj OTA. Sesja zamyka się automatycznie po **120 sekundach**
lub utracie Wi-Fi. Poza oknem serwisowym OTA nie nasłuchuje.

**Ostrzeżenie:** w aktywnym oknie każde urządzenie posiadające dostęp
sieciowy do InfoTAB może próbować przesłać firmware. To wariant
deweloperski na zaufaną sieć lokalną, nie zabezpieczenie produkcyjne.
Docelowo należy wdrożyć podpisy firmware lub uwierzytelnianie bez
konieczności przepisywania hasła oraz ograniczenie dostępu w UniFi.

### Przejście ze starego OTA (v0.7 → v0.8)

Działające v0.7 **nadal wymaga dotychczasowego hasła jednorazowego**.
Dlatego v0.8 wgrywamy **ostatni raz starym, sprawdzonym uploaderem**:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-powershell-syntax.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Użyj kodu pokazanego przez InfoTAB v0.7 po aktywowaniu OTA.
Dopiero po sukcesie i restarcie do v0.8 zaczyna działać tryb bez hasła.

### Kolejne aktualizacje z v0.8 — bez hasła

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Skrypt kompiluje firmware, sprawdza rozmiar dla aktualnego slotu
1280 KiB i prosi o naciśnięcie Enter. Aktywuj OTA na InfoTAB,
naciśnij Enter i skrypt przekaże firmware przez UDP 3232 /
TCP callback 3233. Dotychczasowa ograniczona reguła zapory
Windows dla callback 3233 pozostaje potrzebna.

## Test odbiorczy

- [ ] Build v0.8: SUCCESS, rozmiar obrazu mieści się w 1280 KiB.
- [ ] Przejście OTA v0.7→v0.8 przez stary uploader: SUCCESS.
- [ ] Po restarcie v0.8 pokazuje jaśniejszy interfejs.
- [ ] Ekran UPS ma osiem czytelnych kafelków.
- [ ] Bateria, obciążenie, napięcia i czas pokazują wartości Modbus.
- [ ] Kafelek mocy opisany jako orientacyjny; porównanie skali z UPS osobno.
- [ ] Ekran nie miga przy aktualizacji co 8 sekund.
- [ ] Przyciski i gesty na czterech ekranach działają.
- [ ] Serwis → OTA włącza odbiornik bez hasła na dwie minuty.
- [ ] Następna aktualizacja przez skrypt bez hasła: SUCCESS.

Nie zmieniamy tabeli partycji i nie uruchamiamy migracji 16 MB.
Poprzednie źródła v0.6 są zabezpieczone na osobnej gałęzi.

