// LUMI Pocket - obudowa prototypu (ekran 2,8" SPI na płytce ok. 86x50 mm, kwadratowe przyciski 12x12 THT)
// OpenSCAD. Wszystkie wymiary w mm. Najpierw zmierz swój moduł i popraw sekcję "ZMIERZ".
// Eksport:  openscad -D 'czesc="przod"' -o przod.stl lumi_obudowa.scad   (przod | tyl | przyciski | zlozenie)

czesc = "zlozenie";
$fn = 48;

// ---------- ZMIERZ (moduł ekranu) ----------
ekran_pcb = [86.0, 50.0];        // płytka modułu: szerokość x wysokość (poziomo, piny po lewej)
ekran_pcb_t = 1.6;               // grubość płytki
ekran_lcd_t = 4.2;               // od wierzchu płytki do wierzchu szkła
aktywny = [57.6, 43.2];          // widoczny obraz 2,8" 320x240
aktywny_off = [14.2, 3.4];       // lewy-dolny róg obrazu względem lewego-dolnego rogu płytki
ekran_otwory = [[3.0, 3.0], [83.0, 3.0], [3.0, 47.0], [83.0, 47.0]];  // otwory montażowe na płytce
ekran_otwor_d = 2.2;             // pod wkręt M2.5 samogwintujący (otwór w module ok. 3 mm)

// ---------- przyciski 12x12 THT ----------
sw_body = 12.0;                  // korpus przełącznika
sw_h = 7.3;                      // od płytki do wierzchu popychacza
nasadka_bok = 15.0;              // prawa kolumna (rozstaw 18 mm jak w interfejsie)
nasadka_dol = 18.0;              // dolny rząd: Muzyka, Dom, Dzwoń
nasadka_wystaje = 1.6;           // ile nasadka wystaje ponad front
kolnierz = 1.6;                  // grubość kołnierza nasadki pod frontem (nie wypada)
kolnierz_bok = 1.0;              // wysięg kołnierza w prawej kolumnie (rozstaw 18 mm - mało miejsca)
kolnierz_dol = 1.5;              // wysięg kołnierza w dolnym rzędzie
luz = 0.35;                      // luz nasadki w otworze (druk FDM)
rozstaw_bok = 18.0;
rozstaw_dol = 25.0;

// ---------- obudowa ----------
margines = 7.0;
margines_gora = 12.0;            // więcej u góry: słupek narożny nie może wejść w górny prawy przycisk
sciana = 2.4;
front_t = 2.4;
glebokosc = 30.0;                // wnętrze + front: ESP32 na goldpinach + ogniwo 18650
promien = 8.0;
szczelina = 4.0;                 // między płytką ekranu a prawą kolumną
odstep_dol = 7.0;                // między płytką ekranu a dolnym rzędem

W = margines + ekran_pcb[0] + szczelina + nasadka_bok + margines;
H = margines + nasadka_dol + odstep_dol + ekran_pcb[1] + margines_gora;   // y = 0 na dole frontu
pcb_x = margines;
pcb_y = margines + nasadka_dol + odstep_dol;
akt_c = [pcb_x + aktywny_off[0] + aktywny[0] / 2, pcb_y + aktywny_off[1] + aktywny[1] / 2];
kol_x = pcb_x + ekran_pcb[0] + szczelina + nasadka_bok / 2;
bok_y = [for (i = [1, 0, -1]) akt_c[1] + i * rozstaw_bok];          // górny, środkowy, dolny
dol_y = margines + nasadka_dol / 2;
dol_x = [for (i = [-1, 0, 1]) akt_c[0] + i * rozstaw_dol];          // Muzyka, Dom, Dzwoń

usb = [13.0, 7.0];               // wycięcie USB-C w dolnej ścianie (złącze COM ESP32)
usb_x = 35.0;                    // środek wycięcia od lewej krawędzi
usb_z = 14.0;                    // od frontu w głąb, środek
enkoder_y = dol_y + 2;           // oś rolki w prawej ścianie
enkoder_z = 15.0;
zasil_x = W - 28;                // przycisk zasilania w górnej ścianie
glosnik_d = 36;                  // kratka głośnika na tylnej ściance
sruba_d = 2.6;                   // wkręt M3 samogwintujący w słupkach narożnych
sruby = [[6, 6], [W - 6, 6], [6, H - 5.5], [W - 6, H - 5.5]];

module zaokr(size, r, h) {
  linear_extrude(h) offset(r) offset(-r) square(size);
}

module slupek(h, d_out, d_in) {
  difference() {
    cylinder(h = h, d = d_out);
    translate([0, 0, -0.1]) cylinder(h = h + 0.2, d = d_in);
  }
}

// ---------- przód (front + ściany) - drukować frontem w dół ----------
// Układ liczony "patrząc od frontu" (x w prawo, y w górę). Front leży na z = 0 i patrzy w -Z,
// więc gotową część trzeba odbić w osi X - inaczej prawa kolumna przycisków wyszłaby po lewej.
module przod() { translate([W, 0, 0]) mirror([1, 0, 0]) przod_uklad(); }

module przod_uklad() {
  okno = [aktywny[0] + 1.2, aktywny[1] + 1.2];
  difference() {
    union() {
      difference() {
        zaokr([W, H], promien, glebokosc);
        translate([sciana, sciana, front_t]) zaokr([W - 2 * sciana, H - 2 * sciana], promien - sciana, glebokosc);
      }
      // słupki ekranu: płytka modułu opiera się na nich, szkło dotyka frontu od środka
      // szkło siedzi 0,6 mm w gnieździe frontu, więc słupek jest o tyle krótszy
      for (o = ekran_otwory) translate([pcb_x + o[0], pcb_y + o[1], front_t]) slupek(ekran_lcd_t - 0.6, 6, ekran_otwor_d);
      // słupki narożne na wkręty tylnej ścianki
      for (s = sruby) translate([s[0], s[1], front_t]) slupek(glebokosc - front_t - 2.4, 7, sruba_d);
      // słupki płytek z przyciskami (płytka uniwersalna; wkręty M2)
      p = front_t + kolnierz + sw_h;
      // prawa płytka: słupki między przyciskami, przy ścianie (nad i pod kolumną nie ma miejsca)
      // słupek w połowie wtopiony w ścianę - poza kołnierzem nasadki (min. 0,5 mm luzu)
      for (i = [0, 1]) translate([kol_x + 12, (bok_y[i] + bok_y[i + 1]) / 2, front_t]) slupek(p - front_t, 5, 1.8);
      for (x = [dol_x[0] - 14, dol_x[2] + 14]) translate([x, dol_y, front_t]) slupek(p - front_t, 5, 1.8);
    }
    // okno ekranu z fazą na zewnątrz
    translate([akt_c[0] - okno[0] / 2, akt_c[1] - okno[1] / 2, -0.1]) cube([okno[0], okno[1], front_t + 0.2]);
    translate([akt_c[0], akt_c[1], -0.01])
      linear_extrude(0.8, scale = [okno[0] / (okno[0] + 1.6), okno[1] / (okno[1] + 1.6)])
        square([okno[0] + 1.6, okno[1] + 1.6], center = true);
    // gniazdo na szkło od środka (wycentrowanie modułu)
    translate([pcb_x - 0.3, pcb_y - 0.3, front_t - 0.6]) cube([ekran_pcb[0] + 0.6, ekran_pcb[1] + 0.6, 1]);
    // kwadratowe otwory przycisków
    for (y = bok_y) translate([kol_x, y, front_t / 2]) cube([nasadka_bok + 2 * luz, nasadka_bok + 2 * luz, front_t + 1], center = true);
    for (x = dol_x) translate([x, dol_y, front_t / 2]) cube([nasadka_dol + 2 * luz, nasadka_dol + 2 * luz, front_t + 1], center = true);
    // mikrofon: otwór we froncie przy dolnej krawędzi, między Domem a Dzwoń
    translate([(dol_x[1] + dol_x[2]) / 2, dol_y - 6, -0.1]) cylinder(h = front_t + 0.2, d = 2.2);
    // USB-C w dolnej ścianie
    translate([usb_x, sciana / 2, usb_z]) rotate([90, 0, 0])
      linear_extrude(sciana + 1, center = true) offset(2) offset(-2) square(usb, center = true);
    // rolka (KY-040, gwint M7) w prawej ścianie
    translate([W - sciana / 2, enkoder_y, enkoder_z]) rotate([0, 90, 0]) cylinder(h = sciana + 1, d = 7.4, center = true);
    // przycisk zasilania w górnej ścianie (nasadka 8x8)
    translate([zasil_x, H - sciana / 2, 10]) cube([8 + 2 * luz, sciana + 1, 8 + 2 * luz], center = true);
  }
}

// ---------- tylna ścianka - drukować płasko ----------
module tyl() {
  t = 2.4;
  difference() {
    union() {
      zaokr([W, H], promien, t);
      // kołnierz wchodzący w obudowę: centruje i usztywnia
      translate([sciana + 0.3, sciana + 0.3, t])
        difference() {
          zaokr([W - 2 * sciana - 0.6, H - 2 * sciana - 0.6], promien - sciana, 3);
          translate([1.6, 1.6, -0.1]) zaokr([W - 2 * sciana - 3.8, H - 2 * sciana - 3.8], promien - sciana - 1.6, 3.2);
        }
      // prowadnice koszyczka 18650 (77 x 21 mm), przy dolnej krawędzi
      for (dy = [0, 22]) translate([W / 2 - 40, 12 + dy, t]) cube([80, 1.6, 6]);
    }
    for (s = sruby) translate([s[0], s[1], -0.1]) {
      cylinder(h = t + 0.2, d = 3.4);
      cylinder(h = 1.6, d1 = 6.4, d2 = 3.4);  // łeb stożkowy
    }
    // kratka głośnika: otwory w siatce sześciokątnej, prawa górna część
    gx = W - 32; gy = H - 32;
    for (i = [-5 : 5], j = [-5 : 5]) {
      x = i * 3.6 + (j % 2) * 1.8; y = j * 3.1;
      if (x * x + y * y < (glosnik_d / 2 - 2) * (glosnik_d / 2 - 2)) translate([gx + x, gy + y, -0.1]) cylinder(h = t + 0.2, d = 2.2, $fn = 12);
    }
    // wycięcia na opaski zaciskowe (ESP32, SIM800L)
    for (x = [24, 60], y = [44, 58]) translate([x, y, -0.1]) cube([3, 1.6, t + 0.2]);
  }
}

// ---------- nasadki przycisków - drukować wierzchem w dół ----------
module nasadka(bok, znak = "", wysieg = 1.5) {
  wys = front_t + nasadka_wystaje;
  union() {
    translate([-bok / 2, -bok / 2, 0]) zaokr([bok, bok], 2, wys);
    translate([-(bok + 2 * wysieg) / 2, -(bok + 2 * wysieg) / 2, wys]) zaokr([bok + 2 * wysieg, bok + 2 * wysieg], 2.5, kolnierz);
    // znak dotykowy na wierzchu (tutaj od spodu wydruku): kropka albo kreska jak na ekranie
    if (znak == "kropka") translate([0, 0, -0.8]) cylinder(h = 0.8, d1 = 2.4, d2 = 3.2);
    if (znak == "kreska") translate([-4, -1, -0.8]) cube([8, 2, 0.8]);
    if (znak == "ramka") translate([0, 0, -0.6]) difference() {
      translate([-(bok - 3) / 2, -(bok - 3) / 2, 0]) zaokr([bok - 3, bok - 3], 1.5, 0.6);
      translate([-(bok - 6) / 2, -(bok - 6) / 2, -0.1]) zaokr([bok - 6, bok - 6], 1, 0.8);
    }
  }
}

module przyciski() {
  translate([0, 0, 0.8]) {
    nasadka(nasadka_bok, "kropka", kolnierz_bok);
    translate([22, 0, 0]) nasadka(nasadka_bok, "", kolnierz_bok);
    translate([44, 0, 0]) nasadka(nasadka_bok, "kreska", kolnierz_bok);
    translate([0, 26, 0]) nasadka(nasadka_dol, "", kolnierz_dol);
    translate([26, 26, 0]) nasadka(nasadka_dol, "", kolnierz_dol);
    translate([52, 26, 0]) nasadka(nasadka_dol, "ramka", kolnierz_dol);  // Dzwoń: ramka (najlepiej zielony filament)
    translate([70, 0, 0]) nasadka(8, "", 1.0);                          // zasilanie
  }
}

if (czesc == "przod") przod();
else if (czesc == "tyl") tyl();
else if (czesc == "przyciski") przyciski();
else {
  // złożenie: front na górze (w stronę +Z), układ jak patrząc od frontu
  color("#2a2c30") translate([W, 0, glebokosc]) rotate([0, 180, 0]) przod();
  color("#3a3c40") translate([0, 0, -2.4]) tyl();
  color("#0b0b0d") translate([akt_c[0] - aktywny[0] / 2, akt_c[1] - aktywny[1] / 2, glebokosc - 0.4]) cube([aktywny[0], aktywny[1], 0.2]);
  // nasadki na swoich miejscach (podgląd)
  for (y = bok_y) color("#3a3d42") translate([kol_x - nasadka_bok / 2, y - nasadka_bok / 2, glebokosc]) zaokr([nasadka_bok, nasadka_bok], 2, nasadka_wystaje);
  for (i = [0 : 2]) color(i == 2 ? "#45c08a" : "#3a3d42")
    translate([dol_x[i] - nasadka_dol / 2, dol_y - nasadka_dol / 2, glebokosc]) zaokr([nasadka_dol, nasadka_dol], 2, nasadka_wystaje);
  color("#9a9a9a") translate([kol_x, bok_y[0], glebokosc + nasadka_wystaje]) cylinder(h = 0.6, d = 3);
  color("#9a9a9a") translate([kol_x - 4, bok_y[2] - 1, glebokosc + nasadka_wystaje]) cube([8, 2, 0.6]);
}
// kontrola odstępów (ostrzeżenie w konsoli OpenSCAD, gdy zmienione wymiary powodują kolizję)
fl_b = nasadka_bok / 2 + kolnierz_bok + luz;  // pół szerokości kołnierza prawej kolumny
fl_d = nasadka_dol / 2 + kolnierz_dol + luz;
if (kol_x + 12 - 2.5 - (kol_x + fl_b) < 0.4) echo("UWAGA: slupek prawej plytki dotyka kolnierza");
if (dol_x[1] - dol_x[0] - 2 * fl_d < 0.4) echo("UWAGA: kolnierze dolnych przyciskow nachodza na siebie");
if (rozstaw_bok - 2 * fl_b < 0.2) echo("UWAGA: kolnierze prawej kolumny nachodza na siebie");
if (H - 5.5 - 3.5 - (bok_y[0] + fl_b) < 0.4) echo("UWAGA: gorny prawy slupek narozny dotyka kolnierza");
if (pcb_y - (dol_y + fl_d) < 0.4) echo("UWAGA: dolne przyciski nachodza na plytke ekranu");
echo(str("Obudowa: ", W, " x ", H, " x ", glebokosc + 2.4, " mm"));
