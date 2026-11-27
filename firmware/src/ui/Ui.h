#pragma once
// Wspólne elementy interfejsu: motyw, pasek etykiet przycisków (prawa krawędź), nakładki.
//
// Zasady dla seniora i osób z niepełnosprawnością:
// - etykieta stoi dokładnie przy przycisku, z ikoną i słowem, nigdy sama ikona;
// - znak na etykiecie powtarza wypukłość na przycisku: kropka = górny, kreska = dolny;
// - kolor nie jest jedyną informacją (słowo + ikona + kształt), ale jest stały:
//   zielony = zadzwoń / tak (jak pierścień przycisku Dzwoń), czerwony = nie / rozłącz;
// - jeden ekran = jedna rzecz; problemy (bateria, zasięg) pokazujemy tylko, gdy są.
#include <lvgl.h>
#include <stdint.h>

namespace ui {

enum class Tone : uint8_t { Neutral, Yes, No, Accent };

struct Palette {
  lv_color_t bg, surface, text, dim, accent, yes, no, yesFill, noFill;
  bool highContrast;
};

struct Fonts {
  const lv_font_t* title;  // imiona, nazwy stacji
  const lv_font_t* body;   // główny tekst (min. 28 px)
  const lv_font_t* small;  // drugorzędny (min. 20 px)
  const lv_font_t* clock;  // zegar ok. 72 px
  const lv_font_t* huge;   // temperatura na ekranie pogody
  const lv_font_t* icon;   // ikony w treści
  const lv_font_t* iconBig;
  const lv_font_t* tileClock;  // zegar w kafelku (60 px: "00:00" musi się zmieścić w ok. 180 px)
};

constexpr int RAIL_W = 108;
constexpr int CONTENT_W = 320 - RAIL_W;

void begin();
void setTheme(uint8_t textSize, bool highContrast);
const Palette& pal();
const Fonts& fonts();

lv_obj_t* content();  // lewa część ekranu
void clearContent();
// Obraz pod całym ekranem (np. tło zegara); nullptr = jednolite tło z palety
void setBackground(const void* img);
void setContentWide(bool wide);  // cały ekran (gdy pasek etykiet jest pusty, np. parowanie)

// Pasek etykiet: slot 0 = prawy górny, 1 = środkowy, 2 = dolny
void setRail(int slot, const char* icon, const char* text, Tone tone);
void hideRail(int slot);
void hideRails();
bool railVisible(int slot);
void flashRail(int slot);
void setHighlight(int slot);  // tryb jednego przycisku, -1 = brak
// Etykiety po prawej: domyślnie ukryte (treść na całą szerokość), akcje slotów działają dalej
void setRailLabels(bool show);
bool railLabels();
const char* railText(int slot);  // nullptr, gdy slot pusty
Tone railTone(int slot);
// Pasek podpowiedzi na dole (gdy etykiet nie ma): co robi Dom, a co Dzwoń; nullptr = brak
void hints(const char* home, Tone homeTone, const char* call);

// Elementy treści
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* f, lv_color_t c, const char* text, bool wrap = false);
// Kółko ze zdjęciem (96x96 RGB565) albo z inicjałem, gdy zdjęcia nie ma
lv_obj_t* avatar(lv_obj_t* parent, const char* name, uint32_t color, int size, const lv_image_dsc_t* photo = nullptr);
lv_obj_t* column(lv_obj_t* parent, int gap);
// Kafelek: zaokrąglone pole w kolorze powierzchni, treść w kolumnie (wspólny wzór ekranów)
lv_obj_t* tile(lv_obj_t* parent);
lv_obj_t* row(lv_obj_t* parent, int gap);  // rząd kafelków o równej szerokości (dzieci: flex_grow 1)
constexpr int TILE_PAD = 6;
constexpr int TILE_GAP = 6;
bool fits(const char* text, const lv_font_t* f, int width);
// Krój tytułu, a gdy tekst się nie mieści w jednej linii - mniejszy (bez łamania imion i numerów)
const lv_font_t* titleFont(const char* text);

// Nakładki
void notice(const char* icon, const char* text);
bool noticeVisible();
void dismissNotice();
void toast(const char* text, uint32_t ms = 2500);
void volumeBar(const char* caption, uint8_t level, uint8_t max);
void powerHold(bool show, uint32_t heldMs, uint32_t totalMs);

}  // namespace ui
