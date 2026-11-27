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

// ---------- z projektu v2.2 ----------
W = 105; H = 80; R = 8;
przod_h = 15;                    // wysokość frontu ze ścianami
plyta = 2.0;                     // grubość płyty frontu
sciana = 1.8;
okno = [58.6, 44.2]; okno_c = [-11.0, 9.5];
kol_x = 39.0; kol_w = 19.0; kol_h = 16.13;
kol_y = [28.335, 9.2, -9.935];   // górny, środkowy, dolny
dol_y = -29.0; dol_w = 30.33; dol_h = 14.0;
dol_x = [-33.33, 0, 33.33];      // Muzyka, Dom, Dzwoń
mikrofon = [43.0, -20.0, 1.6];   // [x, y, średnica]
dioda = [36.0, -20.0, 1.0];
sruby = [[-48.6, 36.1], [48.6, 36.1], [-48.6, -36.1], [48.6, -36.1]];
// wycięcia w ścianach: [ściana, środek wzdłuż ściany, szerokość]; wysokość i położenie w głąb wspólne
wyciecia = [
  ["gora", -36.5, 6.6],          // ok. 6 mm - przycisk zasilania?
  ["gora", 34.95, 16.2],         // podłużne - rolka głośności?
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
kolnierz_wysieg = 1.0;           // między otworami jest tylko 3 mm - więcej się nie zmieści
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
      translate([W / 2 - sciana - 2.2, -20, plytka_z - 3]) cube([2.2, 40, 3]);
      translate([-45, -H / 2 + sciana, plytka_z - 3]) cube([90, 2.2, 3]);
    }
    translate([okno_c[0], okno_c[1], -0.1]) linear_extrude(plyta + 0.2) zaokr2d(okno, r_okno);
    for (y = kol_y) translate([kol_x, y, -0.1]) linear_extrude(plyta + 0.2) zaokr2d([kol_w, kol_h], r_otwor);
    for (x = dol_x) translate([x, dol_y, -0.1]) linear_extrude(plyta + 0.2) zaokr2d([dol_w, dol_h], r_otwor);
    for (h = [mikrofon, dioda]) translate([h[0], h[1], -0.1]) cylinder(h = plyta + 0.2, d = h[2]);
    for (w = wyciecia) wyciecie(w);
    for (x = kratka_x) translate([x, -H / 2 + sciana / 2, wyc_z]) rotate([0, 0, 90]) szczelina(wyc_h, kratka_w);
    for (s = sruby) translate([s[0], s[1], przod_h - 6.1]) cylinder(h = 6.2, d = 2.6);  // M3 samogwintujący
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
      for (p = [[kol_x + 6, 19], [kol_x + 6, -1], [-20, dol_y], [20, dol_y]])
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

echo(str("LUMI v3 (", wariant, "): ", W, " x ", H, " x ", przod_h + tyl_h, " mm; wnetrze ok. ", przod_h + tyl_h - plyta - tyl_dno, " mm"));
