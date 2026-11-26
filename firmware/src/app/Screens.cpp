// Ekrany LUMI: co jest na ekranie w danym stanie (bez logiki przycisków i zdarzeń - ta jest w App.cpp).
#include <Arduino.h>
#include <LittleFS.h>

#include <map>
#include <string>

#include "app/State.h"
#include "audio/AudioOut.h"
#include "core/AtParse.h"
#include "core/Calendar.h"
#include "core/Pairing.h"
#include "core/Weather.h"
#include "hw/Battery.h"
#include "modem/Modem.h"
#include "net/Identity.h"
#include "net/Net.h"
#include "ui/Icons.h"
#include "ui/Ui.h"

LV_FONT_DECLARE(lumi_b20);
LV_FONT_DECLARE(lumi_r20);

namespace app {
namespace detail {

// ---------- zdjęcia kontaktów (pamięć podręczna w PSRAM) ----------

std::map<std::string, lv_image_dsc_t*> photoCache;

const lv_image_dsc_t* photoFor(const cfg::Contact* k) {
  if (!k || k->photo.empty()) return nullptr;
  auto it = photoCache.find(k->photo);
  if (it != photoCache.end()) return it->second;  // nullptr = jeszcze nie pobrane
  lv_image_dsc_t* dsc = nullptr;
  File f = LittleFS.open(net::photoPath(k->photo).c_str(), "r");
  if (f && f.size() == net::PHOTO_BYTES) {
#ifdef BOARD_HAS_PSRAM
    uint8_t* px = (uint8_t*)ps_malloc(net::PHOTO_BYTES);
#else
    uint8_t* px = (uint8_t*)malloc(net::PHOTO_BYTES);
#endif
    if (px && f.read(px, net::PHOTO_BYTES) == net::PHOTO_BYTES) {
      dsc = new lv_image_dsc_t{};
      dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
      dsc->header.cf = LV_COLOR_FORMAT_RGB565;
      dsc->header.w = 96;
      dsc->header.h = 96;
      dsc->header.stride = 96 * 2;
      dsc->data_size = net::PHOTO_BYTES;
      dsc->data = px;
    } else {
      free(px);
    }
  }
  if (f) f.close();
  photoCache[k->photo] = dsc;
  return dsc;
}

// po zmianie konfiguracji: zwolnij zdjęcia, których nie używa już żaden kontakt
void freeUnusedPhotos() {
  for (auto it = photoCache.begin(); it != photoCache.end();) {
    bool used = false;
    for (const auto& k : C.contacts) used |= k.photo == it->first;
    if (used) {
      ++it;
      continue;
    }
    if (it->second) {
      lv_image_cache_drop(it->second);
      free((void*)it->second->data);
      delete it->second;
    }
    it = photoCache.erase(it);
  }
}

void dropMissingPhotos() {  // po pobraniu nowych zdjęć: zapomnij "brak pliku"
  for (auto it = photoCache.begin(); it != photoCache.end();)
    it = it->second ? std::next(it) : photoCache.erase(it);
}


void callerName(char* out, size_t n) {
  const cfg::Contact* k = contactAt(call.contact);
  if (k)
    strlcpy(out, k->name.c_str(), n);
  else if (call.number[0])
    at::formatNumber(call.number, out, n);
  else
    strlcpy(out, "Numer zastrzeżony", n);
}

void fmtFreq(float f, char* out, size_t n) {
  snprintf(out, n, "%.1f", f);
  char* dot = strchr(out, '.');
  if (dot) *dot = ',';  // polski zapis: 98,8
}

const char* weatherIcon(int code) {
  switch (wx::iconFor(code)) {
    case wx::Icon::Sun: return IC_SUN;
    case wx::Icon::CloudSun: return IC_CLOUD_SUN;
    case wx::Icon::Cloud: return IC_CLOUD;
    case wx::Icon::Fog: return IC_FOG;
    case wx::Icon::Rain: return IC_RAIN;
    case wx::Icon::Shower: return IC_SHOWER;
    case wx::Icon::Snow: return IC_SNOW;
    case wx::Icon::Storm: return IC_BOLT;
  }
  return IC_CLOUD;
}

void firstHighlight() {
  if (!C.oneButton) {
    highlight = -1;
    ui::setHighlight(-1);
    return;
  }
  highlight = -1;
  for (int i = 0; i < 3; i++)
    if (ui::railVisible(i)) {
      highlight = i;
      break;
    }
  ui::setHighlight(highlight);
}

// ---------- ekrany ----------

lv_obj_t* centeredColumn() {
  lv_obj_t* col = ui::column(ui::content(), 6);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return col;
}

void centerText(lv_obj_t* l) { lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0); }

void renderMissed() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  lv_obj_t* col = ui::column(ui::content(), 4);
  ui::label(col, F.body, P.no, IC_PHONE " Nieodebrane", true);
  char name[40];
  int k = C.contactIndexByNumber(missed.number);
  if (k >= 0)
    strlcpy(name, C.contacts[k].name.c_str(), sizeof name);
  else if (missed.number[0])
    at::formatNumber(missed.number, name, sizeof name);
  else
    strlcpy(name, "Numer zastrzeżony", sizeof name);
  ui::label(col, ui::titleFont(name), P.text, name, true);
  char when[48] = "";
  if (missed.at > 1700000000) {
    tm l;
    localtime_r(&missed.at, &l);
    if (missed.count > 1)
      snprintf(when, sizeof when, "o %02d:%02d, razem %d razy", l.tm_hour, l.tm_min, missed.count);
    else
      snprintf(when, sizeof when, "o %02d:%02d", l.tm_hour, l.tm_min);
  }
  if (when[0]) ui::label(col, F.small, P.dim, when, true);

  if (missed.number[0] && modemReady) ui::setRail(0, IC_PHONE, "Oddzwoń", ui::Tone::Yes);
  else ui::hideRail(0);
  ui::hideRail(1);
  ui::setRail(2, IC_CLOSE, "Zamknij", ui::Tone::Neutral);
}

void statusLine(lv_obj_t* parent) {
  // Pokazujemy tylko problemy - gdy wszystko działa, ekran jest czysty
  auto& P = ui::pal();
  auto& F = ui::fonts();
  char b[48] = "";
  lv_color_t c = P.no;
  modem::Status ms = modem::status();
  uint8_t pct = battery::percent();
  if (battery::charging()) {
    snprintf(b, sizeof b, IC_BOLT " Ładuje się, %u%%", pct);
    c = P.yes;
  } else if (pct <= 20) {
    snprintf(b, sizeof b, IC_BAT_1 " Bateria %u%%", pct);
  } else if (modemFailed) {
    strlcpy(b, IC_WARN " Telefon nie działa", sizeof b);
  } else if (modemReady && !simOk) {
    strlcpy(b, IC_WARN " Brak karty SIM", sizeof b);
  } else if (modemReady && !ms.registered) {
    strlcpy(b, IC_SIGNAL " Brak zasięgu", sizeof b);
  }
  if (b[0]) ui::label(parent, F.small, c, b);
}

void renderHome() {
  if (missed.has) {
    renderMissed();
    return;
  }
  auto& P = ui::pal();
  auto& F = ui::fonts();
  lv_obj_t* col = ui::column(ui::content(), 0);
  statusLine(col);

  tm l = now_tm();
  char b[64];
  if (timeValid())
    snprintf(b, sizeof b, "%d:%02d", l.tm_hour, l.tm_min);
  else
    strlcpy(b, "--:--", sizeof b);
  ui::label(col, F.clock, P.text, b);

  if (timeValid()) {
    // "Wtorek wieczorem" w jednej linii, a gdy się nie mieści - pora dnia osobno
    char day[64];
    cal::dayLine(l.tm_wday, l.tm_hour, C.partOfDay, day, sizeof day);
    if (!C.partOfDay || ui::fits(day, F.body, ui::CONTENT_W - 16)) {
      ui::label(col, F.body, P.text, day);
    } else {
      ui::label(col, F.body, P.text, cal::weekday(l.tm_wday));
      ui::label(col, F.small, P.accent, cal::partOfDay(l.tm_hour));
    }
    cal::dateLine(l.tm_mday, l.tm_mon + 1, b, sizeof b);
    ui::label(col, F.small, P.dim, b);
  }

  wx::Forecast f = net::forecast();
  if (C.weather && f.valid) {
    char t[32];
    snprintf(t, sizeof t, "%s %d°", weatherIcon(f.codeNow), (int)lroundf(f.tempNow));
    lv_obj_t* w = ui::label(ui::content(), F.body, P.text, t);
    lv_obj_align(w, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  }

  bool playing = audio::state().playing;
  for (int i = 0; i < 3; i++) {
    const cfg::HomeSlot& s = C.home[i];
    switch (s.kind) {
      case cfg::SlotKind::Contact: {
        const cfg::Contact* k = C.contactById(s.contactId);
        if (k && C.calls) ui::setRail(i, IC_PHONE, k->name.c_str(), ui::Tone::Yes);
        else ui::hideRail(i);
        break;
      }
      case cfg::SlotKind::Media:
        if (mediaOn() && !C.mediaList.empty()) {
          bool music = C.mediaList[mediaIdx].type == cfg::MediaType::Playlist;
          ui::setRail(i, playing ? IC_PAUSE : IC_MUSIC, playing ? "Gra" : (music ? "Muzyka" : "Radio"),
                      playing ? ui::Tone::Accent : ui::Tone::Neutral);
        } else {
          ui::hideRail(i);
        }
        break;
      case cfg::SlotKind::Weather:
        if (C.weather) ui::setRail(i, IC_CLOUD_SUN, "Pogoda", ui::Tone::Neutral);
        else ui::hideRail(i);
        break;
      default: ui::hideRail(i);
    }
  }
}

void renderContacts() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  if (C.contacts.empty()) {
    lv_obj_t* col = centeredColumn();
    centerText(ui::label(col, F.body, P.text, "Brak kontaktów", true));
    centerText(ui::label(col, F.small, P.dim, "Rodzina może je dodać w aplikacji", true));
    ui::hideRails();
    return;
  }
  contactIdx = clampv<int>(contactIdx, 0, C.contacts.size() - 1);
  const cfg::Contact& k = C.contacts[contactIdx];
  lv_obj_t* col = centeredColumn();
  ui::avatar(col, k.name.c_str(), k.color, 96, photoFor(&k));
  centerText(ui::label(col, ui::titleFont(k.name.c_str()), P.text, k.name.c_str(), true));
  char pos[24];
  snprintf(pos, sizeof pos, "%d z %d", contactIdx + 1, (int)C.contacts.size());
  if (C.contacts.size() > 1) ui::label(col, F.small, P.dim, pos);

  bool many = C.contacts.size() > 1;
  if (many) ui::setRail(0, IC_UP, "Poprzedni", ui::Tone::Neutral);
  else ui::hideRail(0);
  ui::setRail(1, IC_PHONE, "Zadzwoń", ui::Tone::Yes);
  if (many) ui::setRail(2, IC_DOWN, "Następny", ui::Tone::Neutral);
  else ui::hideRail(2);
}

void renderMedia() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  if (C.mediaList.empty()) {
    lv_obj_t* col = centeredColumn();
    centerText(ui::label(col, F.body, P.text, "Brak stacji i muzyki", true));
    centerText(ui::label(col, F.small, P.dim, "Rodzina może je dodać w aplikacji", true));
    ui::hideRails();
    return;
  }
  mediaIdx = clampv<int>(mediaIdx, 0, C.mediaList.size() - 1);
  const cfg::Media& m = C.mediaList[mediaIdx];
  audio::State as = audio::state();
  bool current = playingIdx == mediaIdx && as.src != audio::Src::None;
  bool playing = current && as.playing;

  lv_obj_t* col = ui::column(ui::content(), 4);
  const char* icon = m.type == cfg::MediaType::Fm ? IC_RADIO : m.type == cfg::MediaType::Web ? IC_GLOBE : IC_MUSIC;
  ui::label(col, F.iconBig, playing ? P.accent : P.dim, icon);
  ui::label(col, ui::titleFont(m.name.c_str()), P.text, m.name.c_str(), true);
  char b[64];
  if (m.type == cfg::MediaType::Fm) {
    char fr[12];
    fmtFreq(m.freq, fr, sizeof fr);
    snprintf(b, sizeof b, "Radio FM %s", fr);
  } else if (m.type == cfg::MediaType::Web) {
    strlcpy(b, "Radio z internetu", sizeof b);
  } else if (!m.from.empty()) {
    snprintf(b, sizeof b, "Muzyka od: %s", m.from.c_str());
  } else {
    strlcpy(b, "Muzyka", sizeof b);
  }
  ui::label(col, F.small, P.dim, b, true);

  const char* st = nullptr;
  lv_color_t sc = P.accent;
  if (current && as.connecting && as.noWifi) st = "Szukam Wi-Fi...";
  else if (current && as.connecting) st = "Łączę...";
  else if (current && as.failed) {
    st = as.noWifi ? "Brak Wi-Fi" : "Nie udało się włączyć";
    sc = P.no;
  } else if (playing) st = "Gra";
  if (st) ui::label(col, F.body, sc, st);

  bool many = C.mediaList.size() > 1;
  if (many) ui::setRail(0, IC_UP, "Wcześniej", ui::Tone::Neutral);
  else ui::hideRail(0);
  if (playing || (current && as.connecting)) ui::setRail(1, IC_PAUSE, "Pauza", ui::Tone::Accent);
  else ui::setRail(1, IC_PLAY, "Graj", ui::Tone::Accent);
  if (many) ui::setRail(2, IC_DOWN, "Następna", ui::Tone::Neutral);
  else ui::hideRail(2);
}

void renderWeather() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  wx::Forecast f = net::forecast();
  lv_obj_t* col = ui::column(ui::content(), 2);
  if (!f.valid) {
    ui::label(col, F.body, P.text, "Brak prognozy", true);
    ui::label(col, F.small, P.dim, "Pogoda przychodzi przez Wi-Fi", true);
  } else {
    if (!C.place.empty()) ui::label(col, F.small, P.dim, C.place.c_str());
    char b[48];
    lv_obj_t* row = lv_obj_create(col);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);
    ui::label(row, F.iconBig, P.accent, weatherIcon(f.codeNow));
    snprintf(b, sizeof b, "%d°", (int)lroundf(f.tempNow));
    ui::label(row, F.clock, P.text, b);
    ui::label(col, F.small, P.text, wx::textFor(f.codeNow), true);
    if (f.tomorrow.valid) {
      snprintf(b, sizeof b, "Jutro %s %d°/%d°", weatherIcon(f.tomorrow.code), (int)lroundf(f.tomorrow.tMin),
               (int)lroundf(f.tomorrow.tMax));
      ui::label(col, F.body, P.text, b, true);
      const char* hint = wx::hintFor(f.tomorrow.code, f.tomorrow.tMin);
      if (*hint) ui::label(col, F.body, P.accent, hint, true);
      else ui::label(col, F.small, P.dim, wx::textFor(f.tomorrow.code), true);
    }
  }
  ui::hideRail(0);
  ui::hideRail(1);
  ui::setRail(2, IC_BACK, "Wróć", ui::Tone::Neutral);
}

void renderConfirm() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  const cfg::Contact* k = contactAt(confirmIdx);
  if (!k) return;
  lv_obj_t* col = centeredColumn();
  ui::avatar(col, k->name.c_str(), k->color, 80, photoFor(k));
  ui::label(col, F.small, P.dim, "Zadzwonić do:");
  centerText(ui::label(col, ui::titleFont(k->name.c_str()), P.text, k->name.c_str(), true));
  ui::setRail(0, IC_PHONE, "Zadzwoń", ui::Tone::Yes);
  ui::hideRail(1);
  ui::setRail(2, IC_CLOSE, "Nie", ui::Tone::No);
}

// Pierwsze uruchomienie - ekran dla rodziny (senior dostaje urządzenie już ustawione):
// 1) połącz telefon z siecią LUMI i podaj Wi-Fi, 2) zeskanuj kod, żeby dodać LUMI do aplikacji.
void qrBox(lv_obj_t* parent, const std::string& payload, int size) {
  lv_obj_t* frame = lv_obj_create(parent);  // kod QR musi być ciemny na jasnym, żeby telefon go odczytał
  lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(frame, size + 12, size + 12);
  lv_obj_set_style_bg_color(frame, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(frame, 0, 0);
  lv_obj_set_style_radius(frame, 8, 0);
  lv_obj_set_style_pad_all(frame, 6, 0);
  lv_obj_t* qr = lv_qrcode_create(frame);
  lv_qrcode_set_size(qr, size);
  lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
  lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
  lv_qrcode_update(qr, payload.data(), payload.size());
  lv_obj_center(qr);
}

void renderUnpaired() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  net::Cloud c = net::cloud();
  ui::setContentWide(true);
  ui::hideRails();
  const lv_font_t* f = &lumi_b20;
  bool wifiStep = c == net::Cloud::NoWifi || c == net::Cloud::Provisioning;
  if (wifiStep || c == net::Cloud::Unclaimed) {
    // kod QR z lewej, instrukcja z prawej
    qrBox(ui::content(), wifiStep ? pairing::wifiQr(identity::id(), identity::apPassword())
                                  : pairing::pairUrl(identity::server(), identity::id(), identity::pairCode()),
          138);
    lv_obj_align(lv_obj_get_child(ui::content(), 0), LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* col = ui::column(ui::content(), 6);
    lv_obj_set_size(col, 146, lv_pct(100));
    lv_obj_align(col, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    if (wifiStep) {
      ui::label(col, f, P.accent, "Krok 1 z 2");
      ui::label(col, f, P.text, "Zeskanuj aparatem telefonu, żeby połączyć się z LUMI.", true);
      ui::label(col, &lumi_r20, P.dim, "Potem otwórz stronę", true);
      ui::label(col, f, P.text, "192.168.4.1");
    } else {
      char code[24];
      snprintf(code, sizeof code, "Kod: %s", identity::pairCode());
      ui::label(col, f, P.accent, "Krok 2 z 2");
      ui::label(col, f, P.text, "Zeskanuj, żeby dodać LUMI do aplikacji rodziny.", true);
      ui::label(col, &lumi_r20, P.dim, code, true);
    }
    return;
  }
  lv_obj_t* col = centeredColumn();
  const char* title = "";
  const char* sub = nullptr;
  lv_color_t tc = P.text;
  switch (c) {
    case net::Cloud::Ok: title = "Pobieram ustawienia..."; break;
    case net::Cloud::Connecting: title = "Łączę z Wi-Fi..."; break;
    case net::Cloud::NoServer:
      title = "Brak adresu serwera";
      sub = "Podaj go na stronie 192.168.4.1 albo w konsoli: server <adres>";
      break;
    case net::Cloud::Offline:
      title = "Serwer nie odpowiada";
      sub = "Spróbuję ponownie za chwilę";
      break;
    case net::Cloud::AuthError:
      title = "Serwer odrzuca to urządzenie";
      sub = "Usuń je w aplikacji i dodaj ponownie";
      tc = P.no;
      break;
    default: break;
  }
  centerText(ui::label(col, F.body, tc, title, true));
  if (sub) centerText(ui::label(col, F.small, P.dim, sub, true));
}

// Ekran serwisowy (Home 5 s): dla rodziny przy przeprowadzce lub ponownym dodaniu do aplikacji
void renderService() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  net::Cloud c = net::cloud();
  lv_obj_t* col = ui::column(ui::content(), 4);
  if (c == net::Cloud::Provisioning) {
    ui::label(col, &lumi_b20, P.accent, "Wi-Fi z telefonu");
    qrBox(col, pairing::wifiQr(identity::id(), identity::apPassword()), 120);
    ui::label(col, &lumi_r20, P.text, "potem 192.168.4.1");
  } else if (c == net::Cloud::Unclaimed) {
    ui::label(col, &lumi_b20, P.accent, "Dodaj do aplikacji");
    qrBox(col, pairing::pairUrl(identity::server(), identity::id(), identity::pairCode()), 120);
    char code[24];
    snprintf(code, sizeof code, "Kod: %s", identity::pairCode());
    ui::label(col, &lumi_r20, P.text, code);
  } else {
    ui::label(col, &lumi_b20, P.accent, "Informacje");
    ui::label(col, F.small, P.text, identity::id());
    std::string w = net::wifiConnected() ? "Wi-Fi: " + net::wifiSsid() : "Wi-Fi: brak";
    ui::label(col, F.small, P.text, w.c_str(), true);
    ui::label(col, F.small, P.text, net::cloudName(c), true);
    modem::Status m = modem::status();
    char b[64];
    snprintf(b, sizeof b, "%s, zasięg %d/4", modem::vendorName(m.vendor), m.bars);
    ui::label(col, F.small, P.dim, b, true);
  }
  ui::setRail(0, IC_WIFI, "Wi-Fi", ui::Tone::Neutral);
  ui::hideRail(1);
  ui::setRail(2, IC_BACK, "Wróć", ui::Tone::Neutral);
}

void renderReminder() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  lv_obj_t* col = ui::column(ui::content(), 4);
  char t[16];
  const rem::Item& r = reminders.current();
  snprintf(t, sizeof t, "%d:%02d", r.minute / 60, r.minute % 60);
  lv_obj_t* row = lv_obj_create(col);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 10, 0);
  ui::label(row, F.icon, P.accent, IC_BELL);
  ui::label(row, F.body, P.accent, t);
  ui::label(col, F.title, P.text, r.text.c_str(), true);
  ui::setRail(0, IC_CHECK, "Zrobione", ui::Tone::Yes);
  ui::hideRail(1);
  if (reminders.canSnooze()) ui::setRail(2, IC_CLOSE, "Później", ui::Tone::Neutral);
  else ui::hideRail(2);
}

void renderMessage() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  const net::Message& m = inbox.front();
  lv_obj_t* col = ui::column(ui::content(), 4);
  std::string head = std::string(IC_USER " ") + m.author;
  ui::label(col, F.body, P.accent, head.c_str(), true);
  // długi tekst mniejszą czcionką, żeby zmieścił się cały
  const lv_font_t* f = m.text.size() <= 40 ? F.title : (m.text.size() <= 80 ? F.body : F.small);
  ui::label(col, f, P.text, m.text.c_str(), true);
  if (m.at > 1700000000) {
    time_t t = (time_t)m.at;
    tm l;
    localtime_r(&t, &l);
    char b[24];
    snprintf(b, sizeof b, "%d:%02d", l.tm_hour, l.tm_min);
    ui::label(col, F.small, P.dim, b);
  }
  ui::setRail(0, IC_CHECK, "Dobrze", ui::Tone::Yes);  // "Przeczytane" nie mieści się na przycisku
  if (contactIndexByName(m.author) >= 0 && C.calls) ui::setRail(1, IC_PHONE, "Zadzwoń", ui::Tone::Yes);
  else ui::hideRail(1);
  ui::hideRail(2);
}

void renderCall() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  const cfg::Contact* k = contactAt(call.contact);
  char name[40];
  callerName(name, sizeof name);
  lv_obj_t* col = centeredColumn();
  ui::avatar(col, k ? k->name.c_str() : "?", k ? k->color : 0x5A6270, 80, photoFor(k));
  centerText(ui::label(col, ui::titleFont(name), P.text, name, true));
  switch (call.phase) {
    case Phase::Incoming:
      ui::label(col, F.body, P.yes, "dzwoni");
      live = call.autoAt ? ui::label(col, F.small, P.dim, "", true) : nullptr;
      ui::setRail(0, IC_PHONE, "Odbierz", ui::Tone::Yes);
      if (!call.silenced) ui::setRail(1, IC_BELL_SLASH, "Cisza", ui::Tone::Neutral);
      else ui::hideRail(1);
      ui::setRail(2, IC_PHONE_SLASH, "Odrzuć", ui::Tone::No);
      break;
    case Phase::Outgoing:
      ui::label(col, F.body, P.dim, "Łączę...");
      ui::hideRail(0);
      ui::hideRail(1);
      ui::setRail(2, IC_PHONE_SLASH, "Rozłącz", ui::Tone::No);
      break;
    case Phase::Active:
      live = ui::label(col, F.body, P.yes, "Rozmowa");
      if (call.autoAnswered) centerText(ui::label(col, F.small, P.accent, "Rozmowa otwarta, możesz mówić", true));
      ui::hideRail(0);
      ui::hideRail(1);
      ui::setRail(2, IC_PHONE_SLASH, "Rozłącz", ui::Tone::No);
      break;
    default: break;
  }
}

void updateLive() {
  if (!live) return;
  char b[48];
  uint32_t now = millis();
  if (call.phase == Phase::Active) {
    uint32_t s = (now - call.startedAt) / 1000;
    snprintf(b, sizeof b, "Rozmowa %u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
    lv_label_set_text(live, b);
  } else if (call.phase == Phase::Incoming && call.autoAt && call.autoStage == 0) {
    int32_t left = (int32_t)(call.autoAt - now);
    snprintf(b, sizeof b, "Odbiorę sam za %d s", (int)(left > 0 ? (left + 999) / 1000 : 0));
    lv_label_set_text(live, b);
  }
}

void render() {
  live = nullptr;
  ui::clearContent();
  ui::setContentWide(false);
  if (call.phase != Phase::None) renderCall();
  else switch (scr) {
      case Screen::Home: renderHome(); break;
      case Screen::Contacts: renderContacts(); break;
      case Screen::Media: renderMedia(); break;
      case Screen::Weather: renderWeather(); break;
      case Screen::Confirm: renderConfirm(); break;
      case Screen::Unpaired: renderUnpaired(); break;
      case Screen::Service: renderService(); break;
      case Screen::Reminder: renderReminder(); break;
      case Screen::Message:
        if (inbox.empty()) renderHome();
        else renderMessage();
        break;
    }
  updateLive();
  firstHighlight();
}

void go(Screen s) {
  scr = s;
  render();
}


}  // namespace detail
}  // namespace app
