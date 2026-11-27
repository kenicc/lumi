// LUMI Pocket - obudowa v3 na podstawie projektu lumi_p_v2.2 (Creality Print, 23.11.2026)
// Zewnętrzne wymiary, okno ekranu, otwory przycisków i wycięcia w ścianach są przeniesione z v2.2
// (odczytane z przekrojów modelu, dokładność ok. 0,1 mm; wycięcia w ścianach ok. 0,3 mm).
// Nowe w v3: nasadki do kwadratowych/prostokątnych otworów, półki i dociski płytek z przyciskami,
// wkręty od tyłu, wariant "prototyp" z głębszym tyłem na płytki deweloperskie i ogniwo 18650.
//
// Układ "patrząc od frontu": x w prawo, y w górę, środek frontu w (0,0). Front na z = 0.
// Eksport: openscad -D 'czesc="przod"' -o przod.stl lumi_v3.scad   (przod | tyl | przyciski | zlozenie)

czesc = "zlozenie";
wariant = "prototyp";            // "docelowy": tył 7,5 mm jak w v2.2 | "prototyp": głębszy tył na moduły
$fn = 56;

// ---------- moduł ekranu ----------
// Aukcja Allegro "Wyświetlacz LCD TFT 2,8 240x320 SPI 9 pin ST7789" (oferta 18880632600):
// płytka 81 x 55 mm, wysokość modułu 7 mm. Położenia obrazu na płytce aukcja nie podaje -
// do czasu pomiaru zakładam obraz na środku płytki (ZMIERZ: aktywny_off).
ekran_pcb = [81.0, 55.0];        // płytka modułu, poziomo, piny po lewej
ekran_pcb_t = 1.6;
ekran_lcd_t = 4.2;               // od wierzchu płytki do wierzchu szkła (typowe dla 2,8")
ekran_modul_h = 7.0;             // cały moduł z elementami z tyłu (z aukcji) - zapas głębokości
aktywny = [57.6, 43.2];          // świecący obraz 2,8" 320x240 (zgodnie u wszystkich producentów)
aktywny_off = [(81.0 - 57.6) / 2, (55.0 - 43.2) / 2];   // ZAŁOŻENIE: obraz na środku płytki
// Otwory montażowe: puste = mocowanie bez otworów (narożne kątowniki we froncie + dociski z tyłu).
// Po zmierzeniu można wpisać np. [[3, 3], [78, 3], [3, 52], [78, 52]] - wtedy słupki pod wkręty.
ekran_otwory = [];
ekran_otwor_d = 2.2;             // wkręt M2,5 samogwintujący (gdy otwory podane)
pcb_luz = 0.5;                   // luz płytki do ścian i do kołnierzy przycisków

// ---------- styl i przyciski z projektu v2.2 ----------
H = 80; R = 8;
przod_h = 15;                    // wysokość frontu ze ścianami
plyta = 2.0;                     // grubość płyty frontu
sciana = 1.8;
kol_w = 19.0; kol_h = 16.13;     // otwory prawej kolumny
dol_w = 30.33; dol_h = 14.0;     // otwory dolnego rzędu
przerwa = 3.0;                   // między otworami
kolnierz_wysieg = 1.0;           // kołnierz nasadki pod frontem (między otworami jest tylko 3 mm)

// ---------- układ liczony od części (patrząc od frontu, środek w 0,0) ----------
// Szerokość: płytka ekranu + kolumna przycisków OBOK płytki (nasadka nie może naciskać na płytkę).
W = ceil(2 * sciana + pcb_luz + ekran_pcb[0] + pcb_luz + kolnierz_wysieg + kol_w + kolnierz_wysieg + pcb_luz);
pcb_x0 = -W / 2 + sciana + pcb_luz;                 // lewa krawędź płytki ekranu
pcb_y1 = H / 2 - sciana - pcb_luz;                  // górna krawędź płytki ekranu
pcb_y0 = pcb_y1 - ekran_pcb[1];
okno = [aktywny[0] + 1.0, aktywny[1] + 1.0];        // obraz + 0,5 mm z każdej strony (jak w v2.2)
okno_c = [pcb_x0 + aktywny_off[0] + aktywny[0] / 2, pcb_y0 + aktywny_off[1] + aktywny[1] / 2];
kol_x = pcb_x0 + ekran_pcb[0] + pcb_luz + kolnierz_wysieg + kol_w / 2;
kol_y0 = H / 2 - sciana - pcb_luz - kolnierz_wysieg - kol_h / 2;
kol_y = [for (i = [0 : 2]) kol_y0 - i * (kol_h + przerwa)];   // górny, środkowy, dolny
dol_y = -H / 2 + sciana + pcb_luz + kolnierz_wysieg + dol_h / 2;
dol_x = [for (i = [-1 : 1]) i * (dol_w + przerwa)];           // Muzyka, Dom, Dzwoń
// mikrofon i dioda: w wolnym pasie między kolumną a dolnym rzędem
pas_y = (kol_y[2] - kol_h / 2 - kolnierz_wysieg + dol_y + dol_h / 2 + kolnierz_wysieg) / 2;
mikrofon = [kol_x + 4, pas_y, 1.6];   // [x, y, średnica]
dioda = [kol_x - 3, pas_y, 1.0];
sruby = [for (sx = [-1, 1], sy = [-1, 1]) [sx * (W / 2 - 3.9), sy * (H / 2 - 3.9)]];  // 3,9 mm od krawędzi jak w v2.2
// wycięcia w ścianach: [ściana, środek wzdłuż ściany, szerokość]; wysokość i położenie w głąb wspólne
wyciecia = [
  ["gora", -36.5, 6.6],          // ok. 6 mm - przycisk zasilania?
  ["gora", 18.0, 16.2],          // podłużne - rolka głośności? (w v2.2 przy x 35; tu nad ekranem, bo przy
                                 //  x 35 za otworem stoi teraz płytka prawej kolumny)
  ["lewa", -13.0, 5.7],
  ["lewa", -27.0, 6.9],
];
wyc_z = 10.4; wyc_h = 5.8;       // od frontu w głąb: środek i wysokość (zaokrąglone końce)
r_otwor = 2.7; r_okno = 1.2;      // zaokrąglenia narożników otworów i okna (z v2.2)
faza_frontu = 1.3;               // zaokrąglona przednia krawędź obudowy
kratka_x = [16.7, 21.7, 26.7, 31.7, 36.7, 41.7]; kratka_w = 2.4;   // szczeliny w dolnej ścianie
tyl_v22 = 7.5;
kolnierz_tylu = 1.39;            // wysokość/grubość kołnierza tyłu wchodzącego we front

// ---------- nowe w v3 ----------
tyl_h = wariant == "docelowy" ? tyl_v22 : 22.0;   // prototyp: ESP32 na goldpinach + koszyczek 18650
tyl_dno = 2.0;
luz = 0.3;                       // luz nasadki w otworze z każdej strony
wystaje = 1.6;                   // nasadka ponad front
kolnierz = 1.4;                  // grubość kołnierza nasadki (pod płytą frontu)
sw_h = 7.3;                      // przycisk 12x12 THT: od płytki do wierzchu popychacza
pcb_t = 1.6;
plytka_z = plyta + kolnierz + sw_h;   // przednia powierzchnia płytek z przyciskami (od frontu)

module zaokr2d(size, r) { offset(r) offset(-r) square(size, center = true); }
module skorupa(h, w, hh, r) { linear_extrude(h) zaokr2d([w, hh], r); }
module skorupa_frontu(h) {        // przednia krawędź ścięta łagodnie jak w v2.2
  hull() {
    linear_extrude(0.01) zaokr2d([W - 2 * faza_frontu, H - 2 * faza_frontu], R - faza_frontu);
    translate([0, 0, faza_frontu]) linear_extrude(h - faza_frontu) zaokr2d([W, H], R);
  }
}

module szczelina(dlugosc, wys) {  // otwór o zaokrąglonych końcach, oś Y = wzdłuż ściany
  hull() for (s = [-1, 1]) translate([0, s * (dlugosc - wys) / 2, 0]) rotate([0, 90, 0]) cylinder(h = 10, d = wys, center = true);
}

module wyciecie(w) {
  sc = w[0]; c = w[1]; d = max(w[2], wyc_h);
  if (sc == "gora") translate([c, H / 2 - sciana / 2, wyc_z]) rotate([0, 0, 90]) szczelina(d, wyc_h);
  if (sc == "dol") translate([c, -H / 2 + sciana / 2, wyc_z]) rotate([0, 0, 90]) szczelina(d, wyc_h);
  if (sc == "lewa") translate([-W / 2 + sciana / 2, c, wyc_z]) szczelina(d, wyc_h);
  if (sc == "prawa") translate([W / 2 - sciana / 2, c, wyc_z]) szczelina(d, wyc_h);
}

// ---------- front (układ od frontu; do druku odbity, frontem w dół) ----------
module przod_uklad() {
  difference() {
    union() {
      difference() {
        skorupa_frontu(przod_h);
        translate([0, 0, plyta]) skorupa(przod_h, W - 2 * sciana, H - 2 * sciana, R - sciana);
      }
      // narożne słupki na wkręty od tyłu: krótkie, przy krawędzi (płytki przycisków są głębiej w środku)
      for (s = sruby) translate([s[0], s[1], przod_h - 6]) cylinder(h = 6, d = 6.4);
      // półki pod płytki z przyciskami: przy prawej i dolnej ścianie, na głębokości płytek
      kol_dol = kol_y[2] - kol_h / 2 - 2; kol_gora = kol_y[0] + kol_h / 2 + 1;
      translate([W / 2 - sciana - 2.2, kol_dol, plytka_z - 3]) cube([2.2, kol_gora - kol_dol - 6, 3]);  // bez narożnika (słupek wkrętu)
      translate([-(W / 2 - 9), -H / 2 + sciana, plytka_z - 3]) cube([W - 18, 2.2, 3]);
      // słupki modułu ekranu (gdy znane otwory) albo narożne kątowniki ustawiające płytkę
      for (o = ekran_otwory) translate([pcb_x0 + o[0], pcb_y0 + o[1], plyta]) cylinder(h = ekran_lcd_t - 0.6, d = 5.5);
      if (len(ekran_otwory) == 0)
        for (cx = [0, 1], cy = [0, 1]) {
          x = pcb_x0 + cx * ekran_pcb[0]; y = pcb_y0 + cy * ekran_pcb[1];
          sx = cx == 0 ? -1 : 1; sy = cy == 0 ? -1 : 1;
          h = ekran_lcd_t - 0.6 + ekran_pcb_t;  // do tylnej powierzchni płytki
          // prawe kątowniki bez części przy prawej krawędzi: tam od środka są kołnierze nasadek kolumny
          difference() {
            translate([x + sx * 0.3, y + sy * 0.3, plyta]) mirror([cx, 0, 0]) mirror([0, cy, 0])
              translate([-1.6, -1.6, 0]) difference() { cube([8, 8, h]); translate([1.6, 1.6, -0.1]) cube([8, 8, h + 0.2]); }
            if (cx == 1) translate([pcb_x0 + ekran_pcb[0] - 0.5, -H, 0]) cube([20, 2 * H, przod_h]);
          }
        }
    }
    translate([okno_c[0], okno_c[1], -0.1]) linear_extrude(plyta + 0.2) zaokr2d(okno, r_okno);
    for (y = kol_y) translate([kol_x, y, -0.1]) linear_extrude(plyta + 0.2) zaokr2d([kol_w, kol_h], r_otwor);
    for (x = dol_x) translate([x, dol_y, -0.1]) linear_extrude(plyta + 0.2) zaokr2d([dol_w, dol_h], r_otwor);
    for (h = [mikrofon, dioda]) translate([h[0], h[1], -0.1]) cylinder(h = plyta + 0.2, d = h[2]);
    for (w = wyciecia) wyciecie(w);
    for (x = kratka_x) translate([x, -H / 2 + sciana / 2, wyc_z]) rotate([0, 0, 90]) szczelina(wyc_h, kratka_w);
    for (s = sruby) translate([s[0], s[1], przod_h - 6.1]) cylinder(h = 6.2, d = 2.6);  // M3 samogwintujący
    for (o = ekran_otwory) translate([pcb_x0 + o[0], pcb_y0 + o[1], plyta]) cylinder(h = ekran_lcd_t, d = ekran_otwor_d);
    // kątowniki nie mogą wchodzić w okno ani w otwory przycisków (przy dużym przesunięciu obrazu)

    // gniazdo na szkło: 0,6 mm w płycie frontu, w obrysie płytki (centruje moduł)
    translate([pcb_x0 - 0.2 + ekran_pcb[0] / 2, pcb_y0 + ekran_pcb[1] / 2, plyta - 0.6]) linear_extrude(0.7)
      difference() {
        square([ekran_pcb[0] + 0.4, ekran_pcb[1] + 0.4], center = true);
        for (o = ekran_otwory) translate([o[0] - ekran_pcb[0] / 2, o[1] - ekran_pcb[1] / 2]) circle(d = 6);
      }
  }
}

module przod() { mirror([1, 0, 0]) przod_uklad(); }  // front patrzy w -Z: odbicie, żeby układ był jak od frontu

// ---------- tył ----------
module tyl() {
  difference() {
    union() {
      difference() {
        skorupa(tyl_h, W, H, R);
        translate([0, 0, tyl_dno]) skorupa(tyl_h, W - 2 * 2.7, H - 2 * 2.7, R - 2.7);
      }
      // kołnierz wchodzący we front: 4 proste odcinki, narożniki wolne na słupki wkrętów (jak w v2.2)
      translate([0, 0, tyl_h]) intersection() {
        difference() {
          skorupa(kolnierz_tylu + 1, W - 2 * sciana - 0.4, H - 2 * sciana - 0.4, R - sciana);
          translate([0, 0, -0.1]) skorupa(kolnierz_tylu + 1.2, W - 2 * sciana - 0.4 - 2 * kolnierz_tylu, H - 2 * sciana - 0.4 - 2 * kolnierz_tylu, R - sciana - 1);
        }
        union() {
          translate([0, 0, 5]) cube([2 * 46.2, H, 10], center = true);
          translate([0, 0, 5]) cube([W, 2 * 33.7, 10], center = true);
        }
      }
      for (s = sruby) translate([s[0], s[1], 0]) cylinder(h = tyl_h, d = 6.4);
      // dociski płytek z przyciskami: słupki od dna do tylnej strony płytek
      g = przod_h + tyl_h - plytka_z - pcb_t;  // od dna tyłu do płytki
      // dociski narożników płytki ekranu (gdy mocowanie bez otworów); 3 mm od krawędzi płytki
      if (len(ekran_otwory) == 0) {
        ge = przod_h + tyl_h - (plyta - 0.6 + ekran_lcd_t + ekran_pcb_t);
        for (cx = [0, 1], cy = [0, 1])
          translate([pcb_x0 + 3 + cx * (ekran_pcb[0] - 6), pcb_y0 + 3 + cy * (ekran_pcb[1] - 6), 0]) cylinder(h = ge, d = 4);
      }
      for (p = [[kol_x + 6, (kol_y[0] + kol_y[1]) / 2], [kol_x + 6, (kol_y[1] + kol_y[2]) / 2],
                [(dol_x[0] + dol_x[1]) / 2, dol_y], [(dol_x[1] + dol_x[2]) / 2, dol_y]])
        translate([p[0], p[1], 0]) difference() { cylinder(h = g, d = 5); translate([0, 0, g - 1.5]) cylinder(h = 2, d = 2); }
    }
    for (s = sruby) translate([s[0], s[1], -0.1]) {
      cylinder(h = tyl_h + 0.2, d = 3.4);
      cylinder(h = 1.8, d1 = 6.6, d2 = 3.4);  // łeb stożkowy M3
    }
    // szczeliny głośnika w dnie (jak w v2.2: 6 x 22 x 1,6 mm, co 4 mm)
    for (i = [0 : 5]) translate([28.5, -25.2 + i * 4, -0.1]) linear_extrude(tyl_dno + 0.2) square([22, 1.6], center = true);
  }
}

// ---------- nasadki (drukować wierzchem w dół) ----------
module nasadka(w, h, znak = "") {
  a = w - 2 * luz; b = h - 2 * luz; e = kolnierz_wysieg;
  union() {
    linear_extrude(plyta + wystaje) zaokr2d([a, b], max(r_otwor - luz, 0.5));
    translate([0, 0, plyta + wystaje]) linear_extrude(kolnierz) zaokr2d([a + 2 * e, b + 2 * e], 2);
    if (znak == "kropka") translate([0, 0, -0.8]) cylinder(h = 0.8, d1 = 2.4, d2 = 3.2);
    if (znak == "kreska") translate([0, 0, -0.8]) linear_extrude(0.8) square([8, 2], center = true);
    if (znak == "ramka") translate([0, 0, -0.6]) linear_extrude(0.6) difference() {
      zaokr2d([a - 3, b - 3], 1.2); zaokr2d([a - 6, b - 6], 0.8);
    }
  }
}

module przyciski() {
  translate([0, 0, 0.8]) {
    for (i = [0 : 2]) translate([i * 23, 0, 0]) nasadka(kol_w, kol_h, i == 0 ? "kropka" : i == 2 ? "kreska" : "");
    for (i = [0 : 2]) translate([i * 34, 22, 0]) nasadka(dol_w, dol_h, i == 2 ? "ramka" : "");
  }
}

// ---------- złożenie (front w stronę +Z) ----------
module zlozenie() {
  Z = przod_h + tyl_h;
  color("#d9c9a8") translate([0, 0, Z]) rotate([0, 180, 0]) przod();
  color("#cbb994") tyl();
  color("#111") translate([okno_c[0], okno_c[1], Z - 0.3]) linear_extrude(0.2) square([okno[0] - 1, okno[1] - 1], center = true);
  for (y = kol_y) color("#6b6f76") translate([kol_x, y, Z]) linear_extrude(wystaje) zaokr2d([kol_w - 2 * luz, kol_h - 2 * luz], r_otwor - luz);
  for (i = [0 : 2]) color(i == 2 ? "#45c08a" : "#6b6f76") translate([dol_x[i], dol_y, Z]) linear_extrude(wystaje) zaokr2d([dol_w - 2 * luz, dol_h - 2 * luz], r_otwor - luz);
}

if (czesc == "przod") przod();
else if (czesc == "tyl") tyl();
else if (czesc == "przyciski") przyciski();
else zlozenie();

// kontrola: przyciski nie mogą naciskać na płytkę ekranu ani nachodzić na siebie
if (kol_x - kol_w / 2 - kolnierz_wysieg < pcb_x0 + ekran_pcb[0] + 0.2) echo("UWAGA: kolumna przyciskow nad plytka ekranu");
if (dol_y + dol_h / 2 + kolnierz_wysieg > pcb_y0 - 0.2) echo("UWAGA: dolny rzad pod plytka ekranu");
if (kol_y[2] - kol_h / 2 - kolnierz_wysieg < dol_y + dol_h / 2 + kolnierz_wysieg + 0.4) echo("UWAGA: kolumna nachodzi na dolny rzad");
if (dol_x[2] + dol_w / 2 + kolnierz_wysieg > W / 2 - sciana) echo("UWAGA: dolny rzad nie miesci sie w szerokosci");
if (okno_c[0] - okno[0] / 2 < pcb_x0 || okno_c[0] + okno[0] / 2 > pcb_x0 + ekran_pcb[0]) echo("UWAGA: okno poza plytka ekranu - sprawdz aktywny_off");
echo(str("LUMI v3 (", wariant, "): ", W, " x ", H, " x ", przod_h + tyl_h, " mm; wnetrze ok. ", przod_h + tyl_h - plyta - tyl_dno, " mm"));
