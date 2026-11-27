# Projekt UI: co ma być w Figmie

Lista do przygotowania projektu. Każdy ekran z tej listy istnieje już w kodzie, a układ można zmienić dowolnie. Wymagania z części "Twarde ograniczenia" wynikają ze sprzętu i z tego, kim są użytkownicy. Projekt, który ich nie spełni, nie wejdzie do urządzenia.

Zrzuty obecnej wersji wszystkich ekranów: `firmware/sim/out/*.png` (generuje je `firmware/sim/build.sh`).

---

## Część 1: urządzenie (ekran 2,8")

### Twarde ograniczenia

**Ekran**
- Płótno **320 x 240 px**, poziomo. Kolor 16-bitowy (RGB565): gładkie gradienty dostaną pasy, więc lepiej projektować płaskimi kolorami.
- Rysuje to procesor (LVGL), nie przeglądarka. Rozmycia, cienie i przezroczystości warstw kosztują płynność. Zaokrąglenia, obrysy i płaskie wypełnienia są tanie.
- **Dwa motywy: ciemny i jasny** plus tryb wysokiego kontrastu. Każdy ekran projektujesz w obu motywach. Rodzina wybiera motyw w aplikacji: ciemny, jasny albo automatycznie (jasny w dzień, ciemny wieczorem). Wieczorem jasne tło w ciemnym pokoju oślepia i zużywa więcej baterii, dlatego domyślnie zostaje ciemny albo automatyczny.

**Przyciski fizyczne: prawa krawędź**
- Trzy przyciski obok ekranu: **górny, środkowy, dolny**. Na ekranie każdy ma swoją etykietę na wysokości przycisku.
- Środki przycisków na ekranie: **y = 32, 120, 208 px**. Etykieta musi "wskazywać" swój przycisk, czyli przylegać do prawej krawędzi.
- Znaki dotykowe na obudowie: górny ma **kropkę**, dolny **kreskę**, środkowy nic. Na ekranie powtarzamy te znaki, żeby skojarzyć etykietę z przyciskiem pod palcem.
- Inne przyciski poza ekranem: **Muzyka, Dom, Dzwoń, Zasilanie** oraz **rolka głośności**. Nie mają etykiet na ekranie, ale ekran reaguje na ich użycie (głośność, powrót do zegara).

**Czytelność (użytkownik: senior, często słaby wzrok, drżenie dłoni)**
- Najmniejszy tekst: **20 px**, i tylko dla rzeczy drugorzędnych (data, "1 z 4"). Treść główna: 28-48 px. Zegar: 72-96 px.
- Kontrast tekstu do tła: **co najmniej 7:1** dla treści, 4,5:1 dla drugorzędnych.
- Kolor nigdy nie może być jedyną informacją. Odbierz i Odrzuć różnią się kolorem, ale także ikoną i słowem.
- Pełne słowa zamiast skrótów i samych ikon. Etykieta przy przycisku: ikona plus słowo, najwyżej około 9 liter przy 20 px ("Poprzedni" to granica).
- Jedna rzecz na ekran. Bez list przewijanych, bez menu w menu.

**Trzy wielkości tekstu** (rodzina ustawia je w aplikacji): normalny, duży i bardzo duży. Każdy ekran musi mieć wersję dla wszystkich trzech. Przy bardzo dużym mieści się mniej, więc trzeba zdecydować, co znika.

**Tryb wysokiego kontrastu**: biel i żółć na czerni, bez szarości, grube obramowania przycisków. Osobny wariant każdego ekranu.

### Wymiary (płótno 320 x 240 px, w Figmie ramka 320 x 240 w skali 1x)

```
x: 0                         212 | 212           320
   +----------------------------+----------------+  y = 0
   |                            |  przycisk górny |  środek y = 32  (od 0 do 64)
   |   OBSZAR TREŚCI            +----------------+
   |   212 x 240                |                |
   |   margines wewnętrzny 8    | przycisk środek|  środek y = 120 (od 88 do 152)
   |                            +----------------+
   |                            |                |
   |                            |  przycisk dolny |  środek y = 208 (od 176 do 240)
   +----------------------------+----------------+  y = 240
```

| Element | Wymiar |
| --- | --- |
| Płótno | 320 x 240 px |
| Obszar treści | x 0-212, szerokość **212 px**, cała wysokość; margines wewnętrzny 8 px |
| Kolumna etykiet przycisków | x 212-320, szerokość **108 px** |
| Etykieta przycisku | 108 x **64 px**, środki na y = **32 / 120 / 208** (górne krawędzie 0 / 88 / 176) |
| Odstęp między etykietami | 24 px |
| Zaokrąglenie etykiety | dowolne od lewej strony; prawa krawędź prosta, przyklejona do brzegu ekranu (etykieta "wychodzi" z przycisku) |
| Wnętrze etykiety | ikona 24 px u góry, słowo pod spodem; szerokość tekstu najwyżej ok. 96 px |
| Znaki dotykowe na etykiecie | górna: kropka 9 px, dolna: kreska 16 x 4 px, środkowa: nic |
| Ekrany bez przycisków (np. wiadomość, parowanie) | treść może zająć całą szerokość 320 px |
| Ikona połączenia (zegar) | prawy dolny róg obszaru treści, ok. 30 x 24 px: Wi-Fi albo 4 kreski zasięgu |

**Skala tekstu** (px, w trzech wielkościach ustawianych przez rodzinę):

| Rola | Normalny | Duży | Bardzo duży |
| --- | --- | --- | --- |
| Tytuł (imię, nazwa stacji) | 36 | 48 | 48 |
| Treść | 28 | 36 | 36 |
| Drugorzędny (data, "1 z 4") | 20 | 24 | 28 (pogrubiony) |
| Zegar | 72-96 | 72-96 | 72-96 |
| Temperatura na ekranie pogody | 96 | 96 | 96 |
| Etykieta przycisku | 24, a gdy słowo się nie mieści, 20 | 24 / 20 | 24 / 20 |

Na tak małym ekranie w największej wielkości na treść zostaje mało miejsca. Zaznacz w projekcie, co w trybie "bardzo duży" znika albo przechodzi do drugiej linii.

### Kolory: tokeny do zdefiniowania w Figmie

Zdefiniuj je jako zmienne Figmy (Variables) z trzema trybami: **ciemny**, **jasny** i **wysoki kontrast**. Dzięki temu przełączysz cały projekt jednym kliknięciem, a ja przeniosę wartości 1:1.

| Token | Do czego | Obecnie (ciemny) |
| --- | --- | --- |
| `bg` | tło ekranu | #0B0B0D |
| `surface` | etykiety przycisków, karty | #1C1D21 |
| `text` | tekst główny | #F4F1EA |
| `text-dim` | tekst drugorzędny | #A39E94 |
| `accent` | wyróżnienie, pora dnia | #F2B04B |
| `yes` / `yes-fill` | rozmowa, odbierz, zadzwoń | #45C08A / #1D7350 |
| `no` / `no-fill` | rozłącz, odrzuć, błąd | #F2685B / #9E2F28 |
| `focus` | obrys wyróżnionej etykiety (tryb jednego przycisku) | = accent |

Kontrast sprawdź dla każdej pary tekst/tło **w obu motywach**. Wtyczka Figmy "Contrast" albo "Stark" pokazuje wynik od razu: wymagane co najmniej 7:1 dla treści i 4,5:1 dla drugorzędnych.

### Ekrany do zaprojektowania

| # | Ekran | Co musi być | Przyciski (góra / środek / dół) |
| --- | --- | --- | --- |
| 1 | **Zegar (ekran główny)** | godzina, dzień tygodnia, pora dnia ("wieczorem"), data, mała pogoda | 3 skróty ustawiane przez rodzinę: osoba, radio, pogoda |
| 2 | **Kontakt** | zdjęcie albo inicjał w kolorze, imię, "1 z 4" | Poprzedni / Zadzwoń / Następny |
| 3 | **Potwierdzenie** | zdjęcie, "Zadzwonić do: Ania" (chroni przed przypadkowym wybraniem w kieszeni) | Zadzwoń / - / Nie |
| 4 | **Rozmowa przychodząca** | kto dzwoni (zdjęcie, imię albo numer), "dzwoni" | Odbierz / Cisza / Odrzuć |
| 5 | **Przychodząca z auto-odbieraniem** | jak wyżej plus odliczanie "Odbiorę sam za 5 s" | Odbierz / Cisza / Odrzuć |
| 6 | **Rozmowa trwa** | kto, czas rozmowy | - / - / Rozłącz |
| 7 | **Nieodebrane** | kto albo numer, o której | Oddzwoń / - / Zamknij |
| 8 | **Radio / muzyka** | nazwa stacji, typ (FM, internet, piosenki od Ani), stan (gra, łączy, błąd) | Wcześniej / Pauza-Graj / Następna |
| 9 | **Pogoda** | miejscowość, teraz (ikona, temperatura, opis), jutro, rada ("Weź parasol") | - / - / Wróć |
| 10 | **Wiadomość od rodziny** | od kogo, tekst do 120 znaków (musi się zmieścić w największym tekście), kiedy | Dobrze / Zadzwoń / - |
| 11 | **Przypomnienie** | tekst ("Weź leki na ciśnienie"), godzina | Zrobione / - / Później |
| 12 | **Komunikat** | "Ania dodała kontakt Basia", "Bateria słaba (15%). Podłącz ładowarkę." Senior musi wiedzieć, że coś się zmieniło. | OK / - / - |
| 13 | **Parowanie, krok 1** | kod QR do Wi-Fi LUMI plus instrukcja dla rodziny | - |
| 14 | **Parowanie, krok 2** | kod QR do aplikacji plus kod tekstowy | - |
| 15 | **Tryb jednego przycisku** | dla osób, które nie trafiają w przyciski: wyróżnienie przechodzi po kolei po etykietach, jedno naciśnięcie dowolnego przycisku wybiera | wszystkie, z wyróżnieniem |
| 16 | **Ekran serwisowy** | wersja, sieć, modem, bateria (dla rodziny, nie dla seniora) | Wróć |
| 17 | **Test sprzętu** | kroki: ekran (pasy kolorów), przyciski, rolka, modem, Wi-Fi, wynik | według kroku |

### Nakładki (pojawiają się na dowolnym ekranie)

- **Głośność**: po obrocie rolki, słupki 0-10 i liczba, znika po 2 s.
- **Krótki komunikat (toast)**: "Wysłano", "Brak sieci", znika sam.
- **Przytrzymanie zasilania**: kółko, które się wypełnia, i "Trzymaj, aby wyłączyć".
- **Ikona połączenia** (jest od wersji z panelem): Wi-Fi albo 4 kreski zasięgu komórkowego, w prawym dolnym rogu zegara. Do zaprojektowania: stan "brak połączenia".
- **Pasek stanu** (do decyzji): bateria i ładowanie. Mały, ale czytelny, w rogu, który nie koliduje z przyciskami.

### Komponenty do zdefiniowania

- Etykieta przycisku w 4 odmianach: neutralna, akcent, **tak/zadzwoń** (zielona), **nie/rozłącz** (czerwona). Do tego stan naciśnięcia (mignięcie 160 ms) i wyróżnienie (obrys, gdy przycisk czeka na naciśnięcie).
- Awatar: zdjęcie w kole (96 x 96 px) albo inicjał na kolorze wybranym przez rodzinę.
- Ikony: telefon, odbierz, rozłącz, muzyka, radio, dom, pauza, graj, strzałki, OK, zamknij, głośnik, dzwonek wyciszony, bateria (5 stanów), błyskawica (ładowanie), Wi-Fi, zasięg, pogoda (słońce, chmury, deszcz, śnieg, burza, mgła), informacja, ostrzeżenie. Wszystkie w jednym stylu i czytelne przy 24 px.
- Typografia: 3 rozmiary treści, 2 rozmiary cyfr (zegar, temperatura). Krój musi mieć polskie znaki. Musi też wyraźnie odróżniać I, l i 1, bo to wymóg czytelności dla słabowidzących.
- Paleta: tło, powierzchnia, tekst, tekst drugorzędny, akcent, tak, nie. Wszystko w dwóch wersjach: zwykłej i wysokiego kontrastu.

**Ramki w Figmie:** każdy ekran jako osobna ramka 320 x 240, nazwana tak jak w tabeli (np. `01 Zegar`). Warianty jako kopie ramki z dopiskiem, np. `01 Zegar / jasny / duży tekst`. Komponenty (etykieta przycisku, awatar, ikona połączenia) jako komponenty Figmy z wariantami.

**Eksport do mnie:** kolory jako HEX, rozmiary i odstępy w px na płótnie 320 x 240, czcionki jako plik .ttf albo .woff, ikony jako SVG. Bitmapy (zdjęcia, ilustracje) najlepiej nie, bo każda zajmuje pamięć urządzenia.

---

## Część 2: aplikacja rodziny (telefon, PWA)

### Ograniczenia

- Projekt mobilny, **390 px szerokości**. Na komputerze ta sama kolumna, maksymalnie 640 px.
- Tryb jasny i ciemny (według ustawień telefonu).
- Użytkownicy to dzieci i wnuki seniora, często 50+. Tekst co najmniej 16 px, pola dotyku co najmniej 44 px.
- Trzy role widzą różne rzeczy:
  - **opiekun** widzi wszystko;
  - **rodzina** widzi wszystko oprócz zarządzania osobami;
  - **znajomy** nie widzi listy kontaktów, może tylko zaproponować kontakt i wysłać wiadomość.

### Ekrany

| Ekran | Zawartość |
| --- | --- |
| **Logowanie** | e-mail i kod jednorazowy (bez hasła) |
| **Lista urządzeń** | wszystkie LUMI, do których mam dostęp: imię ("Mama"), stan online, bateria |
| **Urządzenie (pulpit)** | stan (online, bateria, zasięg, czy zmiany dotarły), dzisiejsze przypomnienia z wynikiem, menu sekcji |
| **Kontakty** | lista z kolejnością (to kolejność pod przyciskiem Dzwoń), dodaj, zmień, usuń, zdjęcie, auto-odbieranie, propozycje od znajomych do akceptacji |
| **Zaproponuj kontakt** | dla znajomych: imię i numer |
| **Ekran główny** | podgląd ekranu LUMI na żywo plus wybór 3 skrótów |
| **Wiadomości** | wysłanie tekstu na ekran LUMI, historia z potwierdzeniem przeczytania |
| **Przypomnienia** | lista (godzina, tekst, dni, czy wymaga potwierdzenia), historia wykonania |
| **Radio i muzyka** | stacje FM, internetowe, wgrywanie piosenek (MP3) |
| **Wygląd i dźwięk** | wielkość tekstu, kontrast, jasność, wygaszanie, głośności, cisza nocna |
| **Pogoda** | miejscowość |
| **Dostępność** | auto-odbieranie, odporność na drżenie dłoni (czas naciśnięcia, ignorowanie powtórzeń), blokady przycisków, tryb jednego przycisku |
| **Wi-Fi** | sieci, z którymi łączy się LUMI |
| **Osoby** | kto ma dostęp, role, zaproszenia |
| **Parowanie** | dodanie nowego LUMI: skan kodu QR z ekranu, nadanie imienia |
| **Profil** | imię i forma ("Ania dodała" / "Marek dodał"), powiadomienia push, usunięcie konta |
| **Prywatność** | treść prawna (RODO) |

### Stany, o których łatwo zapomnieć

- pusty stan każdej listy (brak kontaktów, brak przypomnień);
- LUMI offline: od kiedy i co robić;
- zmiana zapisana, ale jeszcze niedostarczona do urządzenia ("czeka na LUMI");
- błąd zapisu, brak uprawnień, sesja wygasła;
- pierwsze uruchomienie: zachęta do włączenia powiadomień i dodania do ekranu głównego.

### Komponenty

Przycisk (główny, zwykły, niebezpieczny), pole tekstowe, numer telefonu, przełącznik, suwak, wybór segmentowy (np. wielkość tekstu), karta, wiersz listy z ikoną, plakietka stanu (ok, ostrzeżenie, błąd, akcent), awatar, pasek zapisu przyklejony do dołu, komunikat po zapisie.
