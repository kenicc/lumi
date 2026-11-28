#include "Ui.h"

#include <stdio.h>
#include <string.h>

#include "Icons.h"

LV_FONT_DECLARE(lumi_b20);
LV_FONT_DECLARE(lumi_b24);
LV_FONT_DECLARE(lumi_b28);
LV_FONT_DECLARE(lumi_b36);
LV_FONT_DECLARE(lumi_b48);
LV_FONT_DECLARE(lumi_r20);
LV_FONT_DECLARE(lumi_r24);
LV_FONT_DECLARE(lumi_r28);
LV_FONT_DECLARE(lumi_d60);
LV_FONT_DECLARE(lumi_d72);
LV_FONT_DECLARE(lumi_d96);

namespace ui {

namespace {

Palette P;
Fonts F;

lv_obj_t* scr;
lv_obj_t* contentObj;

// Etykiety przycisków prawej kolumny = kafelki dokładnie naprzeciw przycisków obudowy v2.2/v3
// (hardware/obudowa_v3). Otwory 16,13 mm co 19,13 mm; obraz 43,2 mm = 240 px (5,56 px/mm), środki
// przycisków wypadają na y = 15 / 122 / 228 px. Kafelek ma wysokość przycisku, a skrajne są ucięte
// krawędzią ekranu (przycisk wystaje poza obraz) - przerwy między kafelkami = przerwy między otworami.
constexpr int RAIL_TOP[3] = {0, 77, 183};
constexpr int RAIL_BOT[3] = {60, 166, 240};

struct Slot {
  lv_obj_t* box;
  lv_obj_t* icon;
  lv_obj_t* text;
  lv_obj_t* mark;
  Tone tone;
  bool visible;
};
Slot rail[3];
int highlighted = -1;

// Etykiety przycisków na ekranie: domyślnie ukryte (projekt 27.11 - ekran w całości na kafelki,
// boczne przyciski obudowy robią zawsze to samo: góra / OK / dół). Akcje slotów zostają, tylko
// nie są rysowane. Pokazujemy je w trybie jednego przycisku, gdzie wyróżnienie musi być widać.
bool labelsShown = false;
lv_obj_t* hintObj = nullptr;
constexpr int HINT_H = 36;

lv_obj_t* noticeObj = nullptr;
lv_obj_t* toastObj = nullptr;
lv_timer_t* toastTimer = nullptr;
lv_obj_t* volObj = nullptr;
lv_timer_t* volTimer = nullptr;
lv_obj_t* powerObj = nullptr;
lv_obj_t* powerArc = nullptr;

void noScroll(lv_obj_t* o) {
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t* panel(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  noScroll(o);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  return o;
}

void styleSlot(Slot& s) {
  // neutralne przyciski: ikona w kolorze tekstu - akcent tylko tam, gdzie coś wyróżniamy
  lv_color_t fill = P.surface, fg = P.text, icon = P.highContrast ? P.accent : P.text;
  if (s.tone == Tone::Yes) {
    fill = P.yesFill;
    icon = P.highContrast ? P.yes : P.text;
  } else if (s.tone == Tone::No) {
    fill = P.noFill;
    icon = P.highContrast ? P.no : P.text;
  } else if (s.tone == Tone::Accent) {
    icon = P.accent;
  }
  lv_obj_set_style_bg_color(s.box, fill, 0);
  lv_obj_set_style_bg_color(s.box, lv_color_lighten(fill, 60), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(s.box, LV_OPA_COVER, 0);
  lv_color_t border = s.tone == Tone::Yes ? P.yes : s.tone == Tone::No ? P.no : P.text;
  lv_obj_set_style_border_width(s.box, P.highContrast ? 3 : 0, 0);
  lv_obj_set_style_border_color(s.box, border, 0);
  lv_obj_set_style_text_color(s.text, fg, 0);
  lv_obj_set_style_text_color(s.icon, icon, 0);
  lv_obj_set_style_bg_color(s.mark, P.highContrast ? fg : P.dim, 0);
  lv_obj_set_style_outline_color(s.box, P.accent, 0);
}

// Najwiekszy krój, w którym słowo mieści się na etykiecie
const lv_font_t* railFont(const char* text) {
  static const lv_font_t* const order[] = {&lumi_b24, &lumi_b20};
  for (const lv_font_t* f : order)
    if (fits(text, f, RAIL_W - 10)) return f;
  return &lumi_b20;
}

void toastDone(lv_timer_t*) {
  if (toastObj) lv_obj_add_flag(toastObj, LV_OBJ_FLAG_HIDDEN);
  toastTimer = nullptr;
}

void volDone(lv_timer_t*) {
  if (volObj) lv_obj_add_flag(volObj, LV_OBJ_FLAG_HIDDEN);
  volTimer = nullptr;
}

void unflash(lv_timer_t* t) {
  Slot* s = static_cast<Slot*>(lv_timer_get_user_data(t));
  lv_obj_remove_state(s->box, LV_STATE_PRESSED);
}

}  // namespace

const Palette& pal() { return P; }
const Fonts& fonts() { return F; }

void setTheme(uint8_t size, bool hc) {
  P.highContrast = hc;
  if (hc) {
    // biel na czerni, bez szarości
    P.bg = lv_color_hex(0x000000);
    P.surface = lv_color_hex(0x000000);
    P.text = lv_color_hex(0xFFFFFF);
    P.dim = lv_color_hex(0xFFFFFF);
    P.accent = lv_color_hex(0xFFD400);
    P.yes = lv_color_hex(0x3DFF8A);
    P.no = lv_color_hex(0xFF5C5C);
    P.yesFill = lv_color_hex(0x000000);
    P.noFill = lv_color_hex(0x000000);
  } else {
    // "Grafit": głęboka czerń, ciepła biel, jeden bursztynowy akcent. Kolor niesie znaczenie:
    // zielony = rozmowa, czerwony = rozłącz, bursztyn = to, na co teraz patrzeć. Reszta spokojna.
    P.bg = lv_color_hex(0x0B0B0D);
    P.surface = lv_color_hex(0x1C1D21);
    P.text = lv_color_hex(0xF4F1EA);
    P.dim = lv_color_hex(0xA39E94);  // kontrast 7:1 na tle - czytelne także dla słabszego wzroku
    P.accent = lv_color_hex(0xF2B04B);
    P.yes = lv_color_hex(0x45C08A);
    P.no = lv_color_hex(0xF2685B);
    P.yesFill = lv_color_hex(0x1D7350);
    P.noFill = lv_color_hex(0x9E2F28);
  }
  switch (size) {
    case 0:
      F = {&lumi_b36, &lumi_b28, &lumi_r20, &lumi_d72, &lumi_d96, &lumi_b28, &lumi_b48, &lumi_d60};
      break;
    case 1:
      F = {&lumi_b48, &lumi_b36, &lumi_r24, &lumi_d72, &lumi_d96, &lumi_b36, &lumi_b48, &lumi_d60};
      break;
    default:
      F = {&lumi_b48, &lumi_b36, &lumi_b28, &lumi_d72, &lumi_d96, &lumi_b36, &lumi_b48, &lumi_d60};
      break;
  }
  if (hc && size == 0) F.small = &lumi_b20;  // w wysokim kontraście cienkie litery znikają
  if (!scr) return;
  lv_obj_set_style_bg_color(scr, P.bg, 0);
  for (Slot& s : rail) styleSlot(s);
}

void begin() {
  setTheme(0, false);
  scr = lv_screen_active();
  noScroll(scr);
  lv_obj_set_style_bg_color(scr, P.bg, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, P.text, 0);

  contentObj = panel(scr);
  lv_obj_set_size(contentObj, labelsShown ? CONTENT_W : 320, 240);
  lv_obj_set_pos(contentObj, 0, 0);
  lv_obj_set_style_pad_all(contentObj, 8, 0);

  for (int i = 0; i < 3; i++) {
    Slot& s = rail[i];
    s.box = lv_obj_create(scr);
    noScroll(s.box);
    // wysunięta poza prawą krawędź, żeby prawe narożniki były proste - "wskazuje" przycisk
    lv_obj_set_size(s.box, RAIL_W + 12, RAIL_BOT[i] - RAIL_TOP[i]);
    lv_obj_set_pos(s.box, 320 - RAIL_W, RAIL_TOP[i]);
    lv_obj_set_style_radius(s.box, 18, 0);
    lv_obj_set_style_pad_all(s.box, 0, 0);
    lv_obj_set_style_outline_pad(s.box, 2, 0);

    s.icon = lv_label_create(s.box);
    lv_obj_set_style_text_font(s.icon, &lumi_b24, 0);
    lv_obj_align(s.icon, LV_ALIGN_CENTER, -6, -13);  // ikona i słowo razem na środku kafelka

    s.text = lv_label_create(s.box);
    lv_label_set_long_mode(s.text, LV_LABEL_LONG_DOT);  // za długie imię kończy się "..."
    lv_obj_set_width(s.text, RAIL_W - 8);
    lv_obj_set_style_text_align(s.text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s.text, LV_ALIGN_CENTER, -6, 14);

    // Znak dotykowy przy krawędzi ekranu: kropka (górny), kreska (dolny), nic (środkowy)
    s.mark = lv_obj_create(s.box);
    noScroll(s.mark);
    lv_obj_set_style_border_width(s.mark, 0, 0);
    lv_obj_set_style_bg_opa(s.mark, LV_OPA_COVER, 0);
    // w rogu kafelka przy krawędzi ekranu (12 px kafelka jest za krawędzią): nie wchodzi na tekst
    if (i == 0) {
      lv_obj_set_size(s.mark, 8, 8);
      lv_obj_set_style_radius(s.mark, LV_RADIUS_CIRCLE, 0);
      lv_obj_align(s.mark, LV_ALIGN_TOP_RIGHT, -18, 6);
    } else if (i == 2) {
      lv_obj_set_size(s.mark, 12, 4);
      lv_obj_set_style_radius(s.mark, 2, 0);
      lv_obj_align(s.mark, LV_ALIGN_TOP_RIGHT, -17, 8);
    } else {
      lv_obj_add_flag(s.mark, LV_OBJ_FLAG_HIDDEN);
    }

    s.tone = Tone::Neutral;
    s.visible = false;
    styleSlot(s);
    lv_obj_add_flag(s.box, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t* content() { return contentObj; }

void clearContent() { lv_obj_clean(contentObj); }

void setBackground(const void* img) {
  static const void* current = nullptr;
  if (img == current) return;  // bez zmiany: nie przerysowuj całego ekranu
  current = img;
  lv_obj_set_style_bg_image_src(scr, img, 0);
}

void setContentWide(bool wide) { lv_obj_set_width(contentObj, (wide || !labelsShown) ? 320 : CONTENT_W); }

void setRailLabels(bool show) {
  if (show == labelsShown) return;
  labelsShown = show;
  for (int i = 0; i < 3; i++) {
    if (show && rail[i].visible) lv_obj_remove_flag(rail[i].box, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(rail[i].box, LV_OBJ_FLAG_HIDDEN);
  }
  setContentWide(false);
}

bool railLabels() { return labelsShown; }

const char* railText(int i) { return (i >= 0 && i < 3 && rail[i].visible) ? lv_label_get_text(rail[i].text) : nullptr; }

Tone railTone(int i) { return (i >= 0 && i < 3) ? rail[i].tone : Tone::Neutral; }

// Podpowiedź na dole ekranu, gdy etykiet nie ma: co robią Dom i Dzwoń na tym ekranie.
// Położenie odpowiada przyciskom pod ekranem: Dom pod środkiem-prawą częścią, Dzwoń w prawym rogu.
void hints(const char* home, Tone homeTone, const char* call) {
  if (hintObj) {
    lv_obj_delete(hintObj);
    hintObj = nullptr;
  }
  lv_obj_set_height(contentObj, 240);
  if ((!home && !call) || labelsShown) return;
  hintObj = panel(scr);
  lv_obj_set_size(hintObj, 320, HINT_H);
  lv_obj_set_pos(hintObj, 0, 240 - HINT_H);
  lv_obj_set_style_bg_color(hintObj, P.surface, 0);
  lv_obj_set_style_bg_opa(hintObj, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(hintObj, 10, 0);
  auto put = [](const char* icon, const char* text, lv_color_t c) {
    char b[48];
    snprintf(b, sizeof b, "%s %s", icon, text);
    lv_obj_t* l = lv_label_create(hintObj);
    lv_obj_set_style_text_font(l, &lumi_b24, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, b);
    return l;
  };
  // Dzwoń w prawym rogu (przycisk jest pod prawą krawędzią), Dom zaraz na lewo od niego
  lv_obj_t* right = nullptr;
  if (call) {
    right = put(IC_PHONE, call, P.yes);
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  if (home) {
    lv_obj_t* h = put(IC_HOME, home, homeTone == Tone::No ? P.no : P.text);
    if (right) {
      lv_obj_update_layout(right);
      lv_obj_align_to(h, right, LV_ALIGN_OUT_LEFT_MID, -18, 0);
    } else {
      lv_obj_align(h, LV_ALIGN_RIGHT_MID, 0, 0);
    }
  }
  lv_obj_set_height(contentObj, 240 - HINT_H);
}

void setRail(int i, const char* icon, const char* text, Tone tone) {
  if (i < 0 || i > 2) return;
  Slot& s = rail[i];
  s.tone = tone;
  s.visible = true;
  lv_label_set_text(s.icon, icon ? icon : "");
  lv_label_set_text(s.text, text);
  lv_obj_set_style_text_font(s.text, railFont(text), 0);
  styleSlot(s);
  if (labelsShown) lv_obj_remove_flag(s.box, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_outline_width(s.box, highlighted == i ? 4 : 0, 0);
}

void hideRail(int i) {
  if (i < 0 || i > 2) return;
  rail[i].visible = false;
  lv_obj_add_flag(rail[i].box, LV_OBJ_FLAG_HIDDEN);
}

void hideRails() {
  for (int i = 0; i < 3; i++) hideRail(i);
}

bool railVisible(int i) { return i >= 0 && i < 3 && rail[i].visible; }

void flashRail(int i) {
  if (!railVisible(i)) return;
  lv_obj_add_state(rail[i].box, LV_STATE_PRESSED);
  lv_timer_t* t = lv_timer_create(unflash, 160, &rail[i]);
  lv_timer_set_repeat_count(t, 1);
}

void setHighlight(int i) {
  highlighted = i;
  for (int k = 0; k < 3; k++) lv_obj_set_style_outline_width(rail[k].box, k == i ? 4 : 0, 0);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* f, lv_color_t c, const char* text, bool wrap) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_label_set_text(l, text);
  if (wrap) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
  }
  return l;
}

lv_obj_t* avatar(lv_obj_t* parent, const char* name, uint32_t color, int size, const lv_image_dsc_t* photo) {
  lv_obj_t* c = lv_obj_create(parent);
  noScroll(c);
  lv_obj_set_size(c, size, size);
  lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, P.highContrast ? 3 : 0, 0);
  lv_obj_set_style_border_color(c, P.text, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  if (photo) {
    lv_obj_set_style_clip_corner(c, true, 0);  // zdjęcie przycięte do koła
    lv_obj_t* img = lv_image_create(c);
    lv_image_set_src(img, photo);
    if (size != (int)photo->header.w) lv_image_set_scale(img, 256 * size / photo->header.w);
    lv_obj_center(img);
    return c;
  }
  // pierwsza litera imienia (UTF-8, np. "Ł")
  char initial[5] = {0};
  if (name && *name) {
    int len = ((unsigned char)name[0] < 0x80) ? 1 : ((unsigned char)name[0] < 0xE0 ? 2 : 3);
    memcpy(initial, name, len);
  } else {
    strcpy(initial, "?");
  }
  lv_obj_t* l = lv_label_create(c);
  lv_obj_set_style_text_font(l, size >= 80 ? &lumi_b48 : &lumi_b28, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(l, initial);
  lv_obj_center(l);
  return c;
}

lv_obj_t* column(lv_obj_t* parent, int gap) {
  lv_obj_t* c = panel(parent);
  lv_obj_set_size(c, lv_pct(100), lv_pct(100));
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(c, gap, 0);
  return c;
}

lv_obj_t* tile(lv_obj_t* parent) {
  lv_obj_t* t = panel(parent);
  lv_obj_set_size(t, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(t, P.surface, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(t, 14, 0);
  lv_obj_set_style_pad_all(t, TILE_PAD, 0);
  lv_obj_set_style_pad_left(t, TILE_PAD + 2, 0);
  lv_obj_set_style_border_width(t, P.highContrast ? 2 : 0, 0);  // w wysokim kontraście tło = czerń, więc ramka
  lv_obj_set_style_border_color(t, P.text, 0);
  lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(t, 0, 0);
  return t;
}

lv_obj_t* row(lv_obj_t* parent, int gap) {
  lv_obj_t* r = panel(parent);
  lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(r, gap, 0);
  return r;
}

bool fits(const char* text, const lv_font_t* f, int width) {
  return lv_text_get_width(text, strlen(text), f, 0) <= width;
}

int contentInnerW() { return (labelsShown ? CONTENT_W : 320) - 16; }

const lv_font_t* titleFont(const char* text) {
  int w = contentInnerW();
  if (fits(text, &lumi_b48, w)) return &lumi_b48;  // jak największy - imię ma być czytelne z daleka
  return fits(text, F.title, w) ? F.title : F.body;
}

// Największy krój, w którym tekst mieści się w JEDNEJ linii (imiona, numery, "Rozmowa 12:34").
// Gdy nawet najmniejszy się nie mieści, zwraca nullptr - wtedy trzeba łamać (fitFont).
const lv_font_t* lineFont(const char* text, int w) {
  static const lv_font_t* const order[] = {&lumi_b48, &lumi_b36, &lumi_b28, &lumi_b24};
  for (const lv_font_t* f : order)
    if (fits(text, f, w)) return f;
  return nullptr;
}

// Największy krój, w którym tekst (łamany na linie) mieści się w prostokącie w x h.
// Najmniejszy dopuszczalny to krój drugorzędny (min. 20 px) - mniejszego senior nie przeczyta.
const lv_font_t* fitFont(const char* text, int w, int h) {
  static const lv_font_t* const order[] = {&lumi_b48, &lumi_b36, &lumi_b28, &lumi_b24};
  for (const lv_font_t* f : order) {
    lv_point_t sz;
    lv_text_get_size(&sz, text, f, 0, 0, w, LV_TEXT_FLAG_NONE);
    if (sz.y <= h && sz.x <= w) return f;
  }
  return F.small;
}

// --- nakładki ---

void notice(const char* icon, const char* text) {
  if (noticeObj) lv_obj_delete(noticeObj);
  noticeObj = panel(lv_layer_top());
  lv_obj_set_size(noticeObj, 320, 240);
  lv_obj_set_style_bg_color(noticeObj, P.bg, 0);
  lv_obj_set_style_bg_opa(noticeObj, LV_OPA_COVER, 0);

  lv_obj_t* box = column(noticeObj, 10);
  lv_obj_set_size(box, CONTENT_W, 240);
  lv_obj_set_style_pad_all(box, 12, 0);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  label(box, F.iconBig, P.accent, icon);
  // jedna informacja na ekranie - tak duża, jak się zmieści
  label(box, fitFont(text, CONTENT_W - 24, 240 - 24 - 52 - 10), P.text, text, true);

  // własna etykieta "OK" przy górnym przycisku - każdy przycisk i tak zamyka
  lv_obj_t* ok = lv_obj_create(noticeObj);
  noScroll(ok);
  lv_obj_set_size(ok, RAIL_W + 12, RAIL_BOT[0] - RAIL_TOP[0]);
  lv_obj_set_pos(ok, 320 - RAIL_W, RAIL_TOP[0]);
  lv_obj_set_style_radius(ok, 14, 0);
  lv_obj_set_style_bg_color(ok, P.surface, 0);
  lv_obj_set_style_border_width(ok, P.highContrast ? 3 : 0, 0);
  lv_obj_set_style_border_color(ok, P.text, 0);
  lv_obj_t* l = label(ok, &lumi_b28, P.text, IC_CHECK " OK");
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 10, 0);
}

bool noticeVisible() { return noticeObj != nullptr; }

void dismissNotice() {
  if (!noticeObj) return;
  lv_obj_delete(noticeObj);
  noticeObj = nullptr;
}

void toast(const char* text, uint32_t ms) {
  if (!toastObj) {
    toastObj = lv_obj_create(lv_layer_top());
    noScroll(toastObj);
    lv_obj_set_width(toastObj, CONTENT_W - 12);
    lv_obj_set_height(toastObj, LV_SIZE_CONTENT);
    lv_obj_align(toastObj, LV_ALIGN_BOTTOM_LEFT, 6, -6);
    lv_obj_set_style_radius(toastObj, 12, 0);
    lv_obj_set_style_pad_all(toastObj, 10, 0);
    lv_label_create(toastObj);
  }
  lv_obj_set_style_bg_color(toastObj, P.highContrast ? P.bg : P.surface, 0);
  lv_obj_set_style_border_width(toastObj, 3, 0);
  lv_obj_set_style_border_color(toastObj, P.accent, 0);
  lv_obj_t* l = lv_obj_get_child(toastObj, 0);
  lv_obj_set_style_text_font(l, F.small == &lumi_r20 ? &lumi_b20 : F.small, 0);
  lv_obj_set_style_text_color(l, P.text, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, lv_pct(100));
  lv_label_set_text(l, text);
  lv_obj_remove_flag(toastObj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(toastObj);
  if (toastTimer) lv_timer_delete(toastTimer);
  toastTimer = lv_timer_create(toastDone, ms, nullptr);
  lv_timer_set_repeat_count(toastTimer, 1);
}

void volumeBar(const char* caption, uint8_t level, uint8_t max) {
  if (!volObj) {
    volObj = lv_obj_create(lv_layer_top());
    noScroll(volObj);
    lv_obj_set_size(volObj, CONTENT_W - 12, 92);
    lv_obj_align(volObj, LV_ALIGN_BOTTOM_LEFT, 6, -6);
    lv_obj_set_style_radius(volObj, 12, 0);
    lv_obj_set_style_pad_all(volObj, 8, 0);
    lv_obj_t* cap = lv_label_create(volObj);
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 0);
    for (int i = 0; i < 10; i++) {
      lv_obj_t* seg = lv_obj_create(volObj);
      noScroll(seg);
      lv_obj_set_style_border_width(seg, 0, 0);
      lv_obj_set_style_radius(seg, 3, 0);
      int h = 14 + i * 3;  // rosnące słupki: widać kierunek "głośniej"
      lv_obj_set_size(seg, 14, h);
      lv_obj_align(seg, LV_ALIGN_BOTTOM_LEFT, i * 18, 0);
    }
  }
  lv_obj_set_style_bg_color(volObj, P.highContrast ? P.bg : P.surface, 0);
  lv_obj_set_style_border_width(volObj, 3, 0);
  lv_obj_set_style_border_color(volObj, P.accent, 0);
  lv_obj_t* cap = lv_obj_get_child(volObj, 0);
  char b[48];
  snprintf(b, sizeof b, IC_VOLUME " %s %u", caption, level);
  lv_obj_set_style_text_font(cap, &lumi_b24, 0);
  lv_obj_set_style_text_color(cap, P.text, 0);
  lv_label_set_text(cap, b);
  for (int i = 0; i < 10; i++) {
    lv_obj_t* seg = lv_obj_get_child(volObj, i + 1);
    bool on = i < level;
    bool over = i >= max;  // powyżej limitu ustawionego przez rodzinę
    lv_obj_set_style_bg_color(seg, on ? P.accent : P.dim, 0);
    lv_obj_set_style_bg_opa(seg, on ? LV_OPA_COVER : (over ? LV_OPA_10 : LV_OPA_30), 0);
  }
  lv_obj_remove_flag(volObj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(volObj);
  if (volTimer) lv_timer_delete(volTimer);
  volTimer = lv_timer_create(volDone, 1800, nullptr);
  lv_timer_set_repeat_count(volTimer, 1);
}

void powerHold(bool show, uint32_t heldMs, uint32_t totalMs) {
  if (!show) {
    if (powerObj) lv_obj_delete(powerObj);
    powerObj = nullptr;
    powerArc = nullptr;
    return;
  }
  if (!powerObj) {
    powerObj = panel(lv_layer_top());
    lv_obj_set_size(powerObj, 320, 240);
    lv_obj_set_style_bg_color(powerObj, P.bg, 0);
    lv_obj_set_style_bg_opa(powerObj, LV_OPA_COVER, 0);
    powerArc = lv_arc_create(powerObj);
    lv_obj_set_size(powerArc, 130, 130);
    lv_obj_align(powerArc, LV_ALIGN_TOP_MID, 0, 16);
    lv_arc_set_bg_angles(powerArc, 0, 360);
    lv_arc_set_rotation(powerArc, 270);
    lv_arc_set_range(powerArc, 0, 100);
    lv_obj_remove_style(powerArc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_width(powerArc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(powerArc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(powerArc, P.surface, LV_PART_MAIN);
    lv_obj_set_style_arc_color(powerArc, P.accent, LV_PART_INDICATOR);
    lv_obj_t* ic = label(powerArc, &lumi_b48, P.text, IC_POWER);
    lv_obj_center(ic);
    lv_obj_t* t = label(powerObj, F.body, P.text, "Trzymaj, aby wyłączyć");
    lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -20);
  }
  uint32_t pct = totalMs ? (heldMs * 100) / totalMs : 0;
  lv_arc_set_value(powerArc, pct > 100 ? 100 : pct);
}

}  // namespace ui
