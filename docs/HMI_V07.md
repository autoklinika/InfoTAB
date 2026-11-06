# InfoTAB v0.7 — przegląd czteroekranowego HMI

Status: kod na gałęzi roboczej, Build i test na Tab5 **PENDING**.

## Styl

Panel techniczny / przemysłowy, bez gradientów i ozdobników.
Antracyt, grafit, chłodna biel, stonowany teal. Zieleń i bursztyn
wyłącznie dla stanów i alarmów. Duża typografia, rytmiczna siatka
i marginesy. Bez ikon wyglądających jak losowe grafiki AI.

## Ekrany (1280×720, orientacja pozioma)

1. **START:** stan zasilania, bateria %, obciążenie %, szacowany czas UPS,
   AI Bridge online/offline oraz IP InfoTAB.
2. **UPS:** napięcie wejścia i wyjścia, bateria %, obciążenie %,
   szacowany czas podtrzymania, częstotliwość i dwa paski poziomu.
3. **SIEC / AI:** połączenie Wi-Fi, IP, RSSI, AI Bridge oraz Modbus TCP;
   przycisk do zmiany Wi-Fi na ekranie.
4. **SERWIS:** wersja firmware, uptime, stan OTA i przyciski
   `OTA WIFI`, `DIAGNOSTYKA`, `KONFIG. WIFI`.

Dolny pasek nawigacji jest widoczny na każdym z czterech ekranów.
Kliknięcie zakładki przełącza bezpośrednio na stronę.

## Gesty

- Przesunięcie w lewo: kolejny ekran.
- Przesunięcie w prawo: poprzedni ekran.
- Ruch poziomy minimum 90 pikseli, co najmniej dwukrotnie większy od
  pionowego, maksymalny czas gestu 1500 ms.
- Normalne dotknięcie przycisku nie jest gestem.
- Gesty wyłączone na ekranach hasła, listy SSID oraz podczas OTA —
  nie przerywają wprowadzania danych ani aktualizacji.

## Odświeżanie

Pełne rysowanie tylko po przełączeniu strony.
Co 8 sekund odczyt Modbus i test AI Bridge aktualizują wyłącznie pola
pomiarów na aktualnie otwartej stronie. Nie jest wywoływane
`fillScreen()` w cyklicznym odświeżaniu.

## Wdrożenie bez kabla

Na urządzeniu uruchom `OTA WIFI → AKTYWUJ OTA` i przepisz
jednorazowy kod na Windows. W terminalu VS Code:

```powershell
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-powershell-syntax.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-windows.ps1
```

Uploader kompiluje `infotab_tab5` dla **aktualnego układu partycji
1280 KiB**. Migracja partycji 16 MB jest odłożona i nie jest wykonywana.
Jeśli kod nie mieści się w slocie, uploader blokuje instalację.

## Odbiór na sprzęcie

- [ ] `Build SUCCESS`, OTA transfer `SUCCESS`, restart do v0.7.
- [ ] Cztery strony wyświetlają się poprawnie, bez przyciętego tekstu.
- [ ] Swipe lewo/prawo przełącza strony we właściwym kierunku.
- [ ] Zakładki na dole działają, bez przypadkowych kliknięć.
- [ ] Ekran nie miga co 5/8 sekund.
- [ ] Modbus pokazuje dane UPS; AI Bridge i Wi-Fi zachowują działanie.
- [ ] Z ekranu Sieć można przejść do konfiguracji Wi-Fi i wrócić.
- [ ] OTA WIFI i diagnostyka dostępne z Serwisu.

Wersja jest przeznaczona do przeglądu na urządzeniu i iteracyjnego
dopracowania ergonomii — nie do zamknięcia jako finalny projekt graficzny.
