# Obudowa LUMI v3 (styl lumi_p_v2.2, wymiary od części)

![Widok od frontu](front.png)

![Złożenie](podglad.png)

Model `lumi_v3.scad` (OpenSCAD) przenosi projekt **lumi_p_v2.2** (Creality Print, 23.11.2026) i dodaje to, czego v2.2 nie miał w środku.

## Wymiary liczone od części (wersja z 28.11)

Obudowa jest dopasowana do **części, a nie do interfejsu**. Interfejs nie ma już etykiet przy przyciskach, więc okno nie musi stać w konkretnym miejscu względem przycisków.

| Element | Skąd wymiar |
| --- | --- |
| **Moduł ekranu 86 x 50 mm** | przy lewej i górnej ścianie, luz 0,5 mm; 4 słupki pod jego otwory, szkło w gnieździe 0,6 mm we froncie |
| **Prawa kolumna** (3 otwory 19 x 16,13) | **obok** płytki ekranu, nie nad nią, bo nasadka nie może naciskać na płytkę; 1,5 mm od krawędzi płytki |
| **Dolny rząd** (3 otwory 30,33 x 14) | pod ekranem, na całą szerokość |
| **Szerokość** | wynika z części: 2 ściany + płytka 86 mm + kolumna z kołnierzami = **113 mm** (w v2.2 było 105, a moduł 86 mm się nie mieścił) |
| **Wysokość** | **80 mm**, jak w v2.2; mieści kolumnę i dolny rząd z 3 mm zapasu |
| Mikrofon i dioda | w wolnym pasie między kolumną a dolnym rzędem |

**Obudowa: 113 x 80 mm**, grubość 22,5 mm (wariant docelowy) albo 37 mm (prototyp).

Z v2.2 zostało bez zmian:
- narożniki R8, front 15 mm, płyta 2 mm, ściany 1,8 mm, zaokrąglona przednia krawędź;
- wymiary i zaokrąglenia otworów przycisków (R2,7), okno = obraz + 0,5 mm (R1,2);
- wycięcia w ścianach, kratka w dolnej ścianie;
- tył z kołnierzem (narożniki wolne) i szczelinami, wkręty 3,9 mm od krawędzi.

**Jedna zmiana w ścianach:** podłużne wycięcie w górnej ścianie (przypuszczalnie na rolkę) przesunąłem nad ekran (x 18). W v2.2 było przy x 35, a tam za ścianą stoi teraz płytka prawej kolumny.

### Zmierz swój moduł ekranu

Moduły 2,8" różnią się między producentami. Popraw sekcję `moduł ekranu` na początku `lumi_v3.scad`:

| Parametr | Co zmierzyć | Domyślnie |
| --- | --- | --- |
| `ekran_pcb` | płytka: szerokość x wysokość (piny po lewej) | 86 x 50 |
| `ekran_lcd_t` | od wierzchu płytki do wierzchu szkła | 4,2 |
| `aktywny_off` | od lewego dolnego rogu płytki do lewego dolnego rogu świecącego obrazu | 14,2 / 3,4 |
| `ekran_otwory` | środki 4 otworów montażowych od lewego dolnego rogu płytki | 3/3, 83/3, 3/47, 83/47 |

Szerokość, okno, kolumna i otwory przeliczą się same. Gdy coś na siebie nachodzi, OpenSCAD wypisze `UWAGA: ...`. Możesz też podać mi te wymiary, a przeliczę model.

## Co nowego w v3 (oprócz wymiarów)

| Zmiana | Po co |
| --- | --- |
| **Nasadki** do otworów: 3 x 18,4 x 15,5 mm i 3 x 29,7 x 13,4 mm, luz 0,3 mm | przyciski w kształcie otworów z v2.2 |
| **Znaki dotykowe** | górny boczny: kropka (w górę); dolny boczny: kreska (w dół); "Dzwoń": wypukła ramka |
| **Kołnierz nasadki 1 mm** | nasadka nie wypada |
| **Półki i słupki dociskowe** | płytki z przyciskami leżą na półkach przy ścianach, tył dociska je po zamknięciu |
| **Wkręty od tyłu** | z frontu nie widać wkrętów |
| **Tył w dwóch wariantach** | docelowy 7,5 mm (płaski akumulator, własna płytka PCB) i prototyp 22 mm (ESP32 na goldpinach, SIM800L, 18650) |

## Pliki

| Plik | Druk |
| --- | --- |
| `przod.stl` | frontem w dół, bez podpór; wycięcia w ścianach mostkują się w poziomie (do 16 mm, PETG i PLA dają radę) |
| `tyl_prototyp.stl` / `tyl_docelowy.stl` | dnem w dół, bez podpór |
| `przyciski.stl` | 6 nasadek, wierzchem w dół |

**Druk:** PETG albo PLA, warstwa 0,2 mm, 3 obrysy, wypełnienie 25%. Nasadki warstwą 0,12 mm, wtedy znaki dotykowe wychodzą wyraźnie.

## Składanie

1. Włóż nasadki od środka frontu. Moduł ekranu włóż szkłem w gniazdo frontu i przykręć do 4 słupków wkrętami M2,5 x 6.
2. Przyciski 12 x 12 przylutuj do dwóch pasków płytki uniwersalnej, w położeniach otworów (patrząc od frontu, środek obudowy w 0,0):
   - prawa kolumna: x 42,8, y 28,6 / 9,5 / -9,6;
   - dolny rząd: y -29,7, x -33,3 / 0 / 33,3.

   W pasku prawej kolumny **zetnij prawy górny róg** (około 8 x 8 mm), bo jest tam słupek wkrętu.
3. Połóż paski na półkach przyciskami w stronę frontu.
4. Zamknij tył: słupki dociskowe przytrzymają paski.
5. Wkręty:
   - wariant docelowy: 4 wkręty M3 x 12 samogwintujące do tworzyw;
   - prototyp: 4 wkręty M3 x 28.

## Zmiany w modelu

Na początku `lumi_v3.scad` są wszystkie wymiary z v2.2 i nowe parametry, m.in.:
- `luz` (luz nasadek);
- `kolnierz_wysieg`;
- `tyl_h` przez `wariant` (docelowy albo prototyp);
- lista `wyciecia` w ścianach.

Polecenia:
```
openscad -D 'czesc="przod"' -o przod.stl lumi_v3.scad
openscad -D 'czesc="tyl"' -D 'wariant="prototyp"' -o tyl_prototyp.stl lumi_v3.scad
openscad -D 'czesc="tyl"' -D 'wariant="docelowy"' -o tyl_docelowy.stl lumi_v3.scad
openscad -D 'czesc="przyciski"' -o przyciski.stl lumi_v3.scad
```
