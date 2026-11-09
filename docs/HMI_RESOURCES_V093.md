# InfoTAB v0.9.3 — pięć zakładek i ZASOBY

Data: 2026-11-09. Zmiany na gałęzi roboczej `feature/resources-tab-20261109`.
Gałąź `main` pozostaje nietknięta.

## Nawigacja dolna (1280 × 720)

Kolejność od lewej:
1. AI PLATFORM
2. ZASOBY
3. NAS
4. UPS
5. USTAWIENIA

Każdy element dolnej nawigacji ma identyczny przydział szerokości, a jego
tekst jest wyśrodkowany. Dotyk i gesty poziome korzystają z jednej tablicy
kolejności `MAIN_PAGES`, która jest źródłem prawdy dla wyświetlania
i przechodzenia między ekranami. Przesunięcie w bok przechodzi dokładnie
do sąsiedniej zakładki. Strefa przycisków jest wyznaczana dynamicznie
z rozdzielczości, a nie zakodowana na stałe jako cztery przyciski.

## Ekran ZASOBY

Jest to **pusty ekran przygotowawczy**, utrzymany w stylu HMI:
- tytuł `ZASOBY`, bez topbara;
- pozioma linia pod tytułem, bez kafelków;
- tekst `MONITORING ZASOBOW AI PLATFORM` i
  `ODCZYTY NIE SA JESZCZE SKONFIGUROWANE`.

Na tym etapie **nie ma nowego endpointu ani symulowanych pomiarów**.
Zakres danych i sposób prezentacji ustalimy osobno. Nie zmieniono backendu
InfoTAB ani istniejących modułów odczytu AI, QNAP i UPS.

## Ochrona pozostałych ekranów

- AI PLATFORM nadal prezentuje stan usług z `GET /services`.
- NAS nadal odczytuje `GET /nas`, włącznie ze stanem RAID i backupów.
- UPS nadal wykonuje tylko odczyty Modbus TCP.
- USTAWIENIA nadal udostępniają sieć Wi-Fi, diagnostykę i OTA.
- Nie zmieniono rozkładu partycji, konfiguracji Wi-Fi ani OTA.
- Wersja wyświetlana w USTAWIENIACH: `InfoTAB v0.9.3`.

## Weryfikacja / wgranie

PlatformIO: `pio run -e infotab_tab5`, wynik **PASS**. Rozmiar firmware:
**1 010 208 B** z dostępnego slotu OTA **1 310 720 B**.

Wgrywanie po stronie Windows z repozytorium:
```powershell
git fetch origin
git switch feature/resources-tab-20261109
git pull --ff-only
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\upload-wifi-no-password-windows.ps1
```

Na fizycznym InfoTab: `USTAWIENIA → OTA Wi-Fi → AKTYWUJ OTA`, następnie
potwierdź Enter w PowerShell. Nie przeprowadzono jeszcze OTA ani odbioru
dotyku pięciu przycisków na urządzeniu.

## Test odbiorczy

1. Czy pięć podpisów mieści się w dolnej belce bez nakładania?
2. Czy wybranie ZASOBY przechodzi na ekran pustego modułu?
3. Czy każdy tap otwiera właściwą zakładkę?
4. Czy swipe z AI do ZASOBY do NAS do UPS do USTAWIENIA działa w obie strony?
5. Czy statusy NAS RAID/backup i lista usług AI nadal się odświeżają?
6. Czy OTA, Wi-Fi i Ustawienia działają tak jak przed zmianą?
