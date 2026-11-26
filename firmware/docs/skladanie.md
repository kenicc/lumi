# Składanie pierwszego prototypu na płytce stykowej

Instrukcja dla części z koszyka Allegro (środowisko `allegro`). Prototyp składa się w dwóch etapach:

| Etap | Co działa | Lutowanie |
| --- | --- | --- |
| **A. Ekran i przyciski** | cały interfejs LUMI, przyciski, rolka, Wi-Fi, parowanie z aplikacją, przypomnienia, wiadomości | **nie trzeba**, o ile ESP32 ma wlutowane piny (sprawdź w kroku A0) |
| **B. Rozmowy** | dzwonienie, odbieranie, dzwonek, sygnały przycisków, poziom baterii | SIM800L zwykle przychodzi z **luźną listwą pinów**: 12 punktów lutowania albo wersja z wlutowanymi pinami |

Najpierw zrób cały etap A. Jeśli on działa, a coś nie działa w etapie B, problem jest na pewno w modemie albo jego zasilaniu, a nie w reszcie.

---

## Etap A: ekran, przyciski, rolka (bez lutowania)

### A0. Sprawdź części

- **ESP32-S3 DevKitC-1:** czy listwy z pinami (goldpiny) są wlutowane? Jeśli przyszły luzem w woreczku, trzeba je przylutować (2 x 22 punkty) albo poprosić kogoś o 15 minut pracy. Bez tego płytka nie będzie stykać.
- **Ekran 2,8" ST7789:** 9 pinów zwykle jest już wlutowanych.
- **Enkoder (rolka):** 5 pinów jest wlutowanych.
- **Przyciski 6x6:** wchodzą prosto w płytkę.

Potrzebne z koszyka: płytka stykowa 830, ESP32, ekran, enkoder, 7 przycisków, przewody męsko-żeńskie (do ekranu i enkodera) i męsko-męskie (na płytce). Do tego kabel USB-C do komputera.

### A1. Ułożenie na płytce

```
 płytka 830 pól (widok z góry)
 +---------------------------------------------------------------+
 | + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + | <- górna szyna: 3V3 (czerwona)
 | - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - | <- górna szyna: GND (niebieska)
 |  [=========== ESP32-S3 ===========]   [p1] [p2] [p3] [p4] ... | <- przyciski nad przerwą
 |  ---------- przerwa środkowa ----------------------------------|
 |  [=========== ESP32-S3 ===========]   (druga nóżka przycisku)  |
 | + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + | <- dolna szyna: na etap B (bateria)
 | - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - |
 +---------------------------------------------------------------+
```

1. Wciśnij ESP32 **okrakiem nad środkową przerwą**, przy lewym końcu płytki. Gniazda USB mają wystawać poza krawędź. Z każdej strony zostanie jeden wolny rząd otworów i to wystarczy, żeby wpiąć przewód przy każdym pinie.
2. Połącz pin **3V3** ESP32 z górną czerwoną szyną, a pin **GND** z górną niebieską.
   **Nie podłączaj pinu 5V nigdzie.**
3. Wciśnij 7 przycisków okrakiem nad przerwą, po prawej stronie płytki, z odstępem 2-3 rzędów. Każdy przycisk ma 4 nóżki: dwie po jednej stronie przerwy, dwie po drugiej.

### A2. Przyciski

Jedna nóżka przycisku idzie do pinu ESP32, nóżka po drugiej stronie przerwy do GND (górna niebieska szyna). Rezystorów nie trzeba, bo podciąganie jest w ESP32.

| Przycisk | GPIO |
| --- | --- |
| prawy górny (tak, odbierz) | 15 |
| prawy środkowy (główna akcja) | 16 |
| prawy dolny (nie, rozłącz) | 17 |
| Muzyka | 18 |
| Home | 21 |
| Dzwoń | 39 |
| Włącznik | 2 |

Najlepiej opisz przyciski taśmą albo kawałkiem kartki. Przy teście łatwiej trafić.

### A3. Ekran (przewody męsko-żeńskie)

| Pin ekranu | Gdzie |
| --- | --- |
| VCC | 3V3 (szyna) |
| GND | GND (szyna) |
| CS | GPIO10 |
| RESET | pin **EN** (czasem opisany RST) na ESP32 |
| DC | GPIO9 |
| SDI (MOSI) | GPIO11 |
| SCK | GPIO12 |
| LED | GPIO13 |
| SDO (MISO) | nie podłączaj |

### A4. Rolka (enkoder)

| Pin enkodera | Gdzie |
| --- | --- |
| CLK | GPIO40 |
| DT | GPIO41 |
| + | **3V3** (nie 5V) |
| GND | GND |
| SW | nie podłączaj |

### A5. Wgranie oprogramowania

1. Zainstaluj [VS Code](https://code.visualstudio.com) i w nim rozszerzenie **PlatformIO IDE**.
2. Otwórz folder `firmware` z repozytorium. Na dolnym pasku wybierz środowisko **env:allegro**.
3. Podłącz ESP32 kablem do gniazda opisanego **COM** (na niektórych płytkach **UART**). Przez to gniazdo działa wgrywanie i konsola. Windows pokaże je w Menedżerze urządzeń jako "USB-Enhanced-SERIAL CH343 (COMx)" albo "CP210x". Jeśli nie ma go wcale, zainstaluj sterownik CH343 albo CP210x.
4. Kliknij **Upload**, a potem w menu PlatformIO (allegro → Platform) **Upload Filesystem Image**. Uwaga: nie "Build Filesystem Image", bo ta opcja tylko buduje plik na komputerze. Upload Filesystem Image wgrywa dźwięki i przykładową konfigurację. Monitor musi być wtedy zamknięty, bo zajmuje port.
5. Jeśli wgrywanie się nie zaczyna: przytrzymaj BOOT, naciśnij i puść RST, puść BOOT, spróbuj ponownie.
6. Otwórz **Serial Monitor** (115200) i naciśnij RST. Po kilku sekundach powinna pojawić się linia `[config] wersja 1, kontakty: 4...`, a potem `LUMI allegro gotowe`. Wpisz `help`.

Komunikaty `no matching wifi found!` są normalne, dopóki w `data/config.json` nie ma nazwy i hasła twojej sieci.

**Ekran zostaje czarny albo biały:**
- Program nie potrzebuje do tego ekranu: jeśli w konsoli jest `LUMI allegro gotowe`, ESP32 działa, a problem leży w połączeniu.
- Piny ekranu muszą być **przylutowane**. Listwa tylko włożona w otwory nie ma pewnego styku. Ekran wtedy miga na biało przy ruszaniu albo nie reaguje wcale.
- Biały ekran bez obrazu oznacza, że podświetlenie działa, ale ekran nie dostał poleceń: sprawdź CS, DC, SDA, SCL i RESET, potem naciśnij RST.
- `lcd test` w konsoli rysuje pasy: od lewej czerwony, zielony, niebieski, biały, czarny.
- Kolory w negatywie: `lcd inv`. Czerwony z niebieskim zamienione: `lcd bgr`. Obraz do góry nogami: `lcd flip`. Ustawienie zostaje zapamiętane, nie trzeba nic zmieniać w kodzie.

### A6. Test

Odłącz i podłącz USB, **trzymając Home**. LUMI uruchomi test sprzętu.

| Krok | W etapie A powinno być |
| --- | --- |
| Ekran | ✓ cztery pasy: czerwony, zielony, niebieski, biały |
| Przyciski | ✓ każdy zielenieje po naciśnięciu |
| Rolka | ✓ najpierw w prawo, potem w lewo |
| Modem | ✗ - **normalne**, jeszcze go nie ma; naciśnij górny, żeby pominąć |
| Dźwięk | ✗ - **normalne**, w tym prototypie dźwięk idzie przez modem |
| Bateria | ✗ - **normalne**, napięcie podaje modem |
| Sieć | zależy, czy podałeś Wi-Fi |

Jeśli coś jest nie tak, w konsoli jest dokładna podpowiedź. Najczęstsze przypadki:
- **Biały ekran:** zły przewód SCK, MOSI albo DC.
- **Negatyw:** zmień `-DLUMI_PANEL_INVERT=0` na `1` w `platformio.ini` (środowisko allegro) i wgraj ponownie.
- **Obraz obrócony:** ustaw `LUMI_ROTATION` (0-3).
- **Przycisk nie reaguje:** nóżki są po tej samej stronie przerwy (wtedy przycisk jest stale zwarty albo stale rozwarty) albo przewód trafił do złego pinu.
- **Rolka działa odwrotnie:** zamień CLK z DT.

Po udanym teście cały interfejs da się klikać przyciskami albo z konsoli (`key call`, `msg Cześć`, `remind Weź leki`). Wi-Fi i parowanie z aplikacją działają już w etapie A.

---

## Etap B: rozmowy (SIM800L)

### B0. Czego brakuje w koszyku

- **Głośnik 8 Ω, 0,5-1 W, z przewodami.** Najlepiej taki z przylutowanymi kabelkami: końcówki wchodzą w płytkę.
- **Mikrofon elektretowy** (nóżki wchodzą w płytkę).
- **Ogniwo 18650 z zabezpieczeniem** (PCB). Bez TP4056 tylko takie, bo zabezpieczenie w ogniwie chroni przed zwarciem i rozładowaniem do zera.
- **Karta SIM z 2G** (Orange albo T-Mobile) i adapter nano na micro.
- **Najpierw włóż kartę do telefonu i wyłącz w niej PIN.**
- **Rezystory 10 kΩ:** 3 sztuki, są w zestawie 600 szt.

### B1. Lutowanie (jedyne konieczne)

- **SIM800L:** wlutuj listwę pinów (2 x 6 punktów), krótszą stroną pinów do modułu. To najprostsze możliwe lutowanie. Wystarczy tania lutownica ok. 40-60 W i cyna z topnikiem, a zajmuje 10 minut. Alternatywy: szukaj oferty "SIM800L z wlutowanymi pinami" albo poproś kogoś znajomego.
- **Antena:** jeśli SIM800L ma małe gniazdo antenowe (IPEX, okrągłe), weź antenę z wtykiem IPEX, wtedy bez lutowania. Antena-sprężynka wymaga przylutowania w otwór ANT.
- **TP4056 na razie pomiń** (ma tylko pola do lutowania). Ogniwo ładuj osobno, np. w ładowarce do 18650.

### B2. Zasilanie modemu

SIM800L chwilami pobiera do **2 A**. To najczęstsze źródło problemów na płytce stykowej.

1. Dolną parę szyn przeznacz na baterię: czerwona **VBAT**, niebieska **GND**. Połącz **dolny GND z górnym GND** (wspólna masa jest konieczna).
2. Przewody koszyczka 18650 wepnij w dolne szyny. Jeśli są cienkie i miękkie, skręć końcówki albo pobiel je cyną.
3. Dwa kondensatory **1000 µF** wepnij **tuż przy modemie**, między VBAT a GND. **Biały pasek na kondensatorze = minus = GND.** Odwrotnie wpięty może się nagrzać i wybrzuszyć.
4. **Nigdy nie podłączaj VCC modemu do 5V ani 3V3 ESP32.** 5V go uszkodzi, 3V3 nie wystarczy.

### B3. Połączenia SIM800L

| Pin SIM800L | Gdzie |
| --- | --- |
| VCC | VBAT (dolna czerwona szyna), najkrótszym przewodem |
| GND | GND |
| RXD | przez dzielnik: **GPIO47 - 10 kΩ - RXD - 10 kΩ - 10 kΩ - GND** (SIM800L ma logikę 2,8 V, ESP32 daje 3,3 V) |
| TXD | GPIO48 (DevKit v1.1) albo GPIO38 (v1.0, wtedy dodaj `-DLUMI_DEVKIT_V10`) |
| RST | GPIO42 |
| RING | GPIO3 |
| DTR | GPIO38 (v1.1) albo GPIO48 (v1.0) |
| SPK+ / SPK- | głośnik |
| MIC+ / MIC- | mikrofon. Nóżka połączona z obudową mikrofonu to minus (MIC-) |
| NET | nie podłączaj - to tylko dioda na module |

Wersję DevKitu (v1.0 albo v1.1) zwykle widać na spodzie płytki albo w opisie oferty. Jeśli nie wiesz, zacznij od ustawień v1.1. Gdy modem nie odpowiada, zamień TXD z DTR, czyli dodaj flagę `-DLUMI_DEVKIT_V10`.

### B4. Kolejność włączania

1. Włóż kartę SIM (ścięty róg zgodnie z rysunkiem na gnieździe).
2. Włóż ogniwo do koszyczka. Dioda NET na modemie zaczyna mrugać.
3. Podłącz USB do ESP32.
4. W konsoli: `status`, potem `diag`.

| Dioda NET | Znaczenie |
| --- | --- |
| mruga co ok. 1 s | szuka sieci |
| mruga co ok. 3 s | **zalogowany, gotowy do rozmów** |
| szybko, a potem gaśnie i od nowa | **za słabe zasilanie**: krótsze i grubsze przewody do VCC i GND, kondensatory bliżej, naładowane ogniwo |
| nie świeci | brak zasilania na VCC |

5. Test: przytrzymaj Home przy włączaniu. Krok "Modem" powinien pokazać "SIM800 (2G)" i operatora.
6. Rozmowa: `call 600100200` (Twój numer) albo przycisk Dzwoń na LUMI.

### B5. Gdy rozmowa działa

- Głośność rozmowy reguluje rolka, a dzwonek ustawia aplikacja albo rolka poza rozmową.
- **Słychać echo:** odsuń mikrofon od głośnika (na płytce choćby 10 cm) albo włóż głośnik w kubek.
- **Za cicho:** w konsoli `at AT+CLVL=90` (zakres 0-100).

---

## Bezpieczeństwo

- **Ogniwo Li-ion:** tylko z zabezpieczeniem, nigdy nie zwieraj biegunów, nie zostawiaj ładowania bez nadzoru, nie używaj uszkodzonego ani wybrzuszonego.
- **Kondensatory elektrolityczne** podłączaj zgodnie z oznaczeniem minusa.
- **Piny ESP32** przyjmują najwyżej 3,3 V. Żaden przewód z baterii ani z 5V nie może trafić na GPIO.
- **Przepinanie przewodów** rób przy odłączonym USB i wyjętym ogniwie.

## Co dalej

Gdy oba etapy działają, kolejny krok to przeniesienie na płytkę uniwersalną (lutowaną) i obudowę. Wtedy dochodzi TP4056 do ładowania i stabilizator 3,3 V, żeby całość działała bez kabla USB.
