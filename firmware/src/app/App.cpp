#include "app/App.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

#include <atomic>
#include <deque>
#include <string>

#include "app/Diag.h"
#include "audio/AudioOut.h"
#include "core/AtParse.h"
#include "core/Calendar.h"
#include "core/Pairing.h"
#include "hw/Battery.h"
#include "hw/Board.h"
#include "hw/Display.h"
#include "hw/Input.h"
#include "modem/Modem.h"
#include "net/Identity.h"
#include "net/Ota.h"
#include "net/Net.h"
#include "ui/Icons.h"
#include "ui/Ui.h"

LV_FONT_DECLARE(lumi_b20);
LV_FONT_DECLARE(lumi_r20);

namespace app {

namespace {

enum class Screen : uint8_t { Home, Contacts, Media, Weather, Confirm, Unpaired, Service };
enum class Phase : uint8_t { None, Incoming, Outgoing, Active };

constexpr uint32_t POWER_LONG_MS = 3000;
constexpr uint32_t CONFIRM_TIMEOUT_MS = 10000;

cfg::Config C;
bool haveConfig = false;
char devId[24];

Screen scr = Screen::Home;
int contactIdx = 0, mediaIdx = 0, confirmIdx = -1;
int playingIdx = -1;  // pozycja listy mediów, która gra
uint32_t confirmAt = 0;

struct Call {
  Phase phase = Phase::None;
  char number[24] = {0};
  int contact = -1;
  uint32_t startedAt = 0;
  bool silenced = false;
  bool rejected = false;
  bool hungUpHere = false;  // rozłączył senior (bez komunikatu "Nikt nie odebrał")
  uint32_t autoAt = 0;  // automatyczne odbieranie
  uint8_t autoStage = 0;
  bool autoAnswered = false;
};
Call call;

struct Missed {
  bool has = false;
  char number[24] = {0};
  time_t at = 0;
  int count = 0;
};
Missed missed;

std::deque<std::string> notices;
std::string shownNotice;  // aktualnie widoczny - wraca do kolejki, gdy przerwie go rozmowa
Preferences prefs;

uint32_t lastInput = 0;
bool screenOn = true;
uint8_t volume = 5, callVolume = 7;
int highlight = -1;
int lastMinute = -1;
uint32_t lastSecond = 0, lastTick = 0;
bool modemReady = false, simOk = true, modemFailed = false;
bool powerHolding = false;
uint32_t powerHoldAt = 0;
uint8_t batteryWarned = 100;
uint8_t ringOverride = 0;  // prototyp bez radia: rolka poza rozmową zmienia głośność dzwonka (0 = jak w aplikacji)
lv_obj_t* live = nullptr;
std::atomic<bool> idleFlag{false};
std::atomic<bool> callFlag{false};
bool healthyMarked = false;  // etykieta odświeżana co sekundę (czas rozmowy, odliczanie)

// ---------- pomocnicze ----------

bool mediaOn() { return C.media && audio::hasMedia(); }

uint8_t ringLevel() { return ringOverride ? ringOverride : C.ringVolume; }

const cfg::Contact* contactAt(int i) { return (i >= 0 && i < (int)C.contacts.size()) ? &C.contacts[i] : nullptr; }

bool timeValid() { return time(nullptr) > 1700000000; }

tm now_tm() {
  time_t t = time(nullptr);
  tm l;
  localtime_r(&t, &l);
  return l;
}

int minuteOfDay() {
  tm l = now_tm();
  return l.tm_hour * 60 + l.tm_min;
}

template <typename T>
T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void applyBacklight() {
  if (screenOn)
    display::setBacklight(C.brightness, audio::fmActive());
  else
    display::off();
}

void wake() {
  lastInput = millis();
  if (!screenOn) {
    screenOn = true;
    applyBacklight();
  }
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
  ui::avatar(col, k.name.c_str(), k.color, 96);
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
  if (many) ui::setRail(0, IC_UP, "Poprzednia", ui::Tone::Neutral);
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
  ui::avatar(col, k->name.c_str(), k->color, 80);
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

void renderCall() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  const cfg::Contact* k = contactAt(call.contact);
  char name[40];
  callerName(name, sizeof name);
  lv_obj_t* col = centeredColumn();
  ui::avatar(col, k ? k->name.c_str() : "?", k ? k->color : 0x5A6270, 80);
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
    }
  updateLive();
  firstHighlight();
}

void go(Screen s) {
  scr = s;
  render();
}

// ---------- działania ----------

void startCall(const char* number, int contact) {
  if (!modemReady) {
    ui::toast("Telefon jeszcze się uruchamia. Spróbuj za chwilę.");
    audio::sound(audio::Sound::Error);
    return;
  }
  if (!simOk) {
    ui::toast("Brak karty SIM");
    audio::sound(audio::Sound::Error);
    return;
  }
  call = Call{};
  call.phase = Phase::Outgoing;
  strlcpy(call.number, number, sizeof call.number);
  call.contact = contact;
  audio::callMode(true);  // radio cichnie, sygnał łączenia słychać z modemu
  modem::dial(number);
  render();
}

void playMedia(int idx) {
  if (idx < 0 || idx >= (int)C.mediaList.size()) return;
  const cfg::Media& m = C.mediaList[idx];
  playingIdx = idx;
  if (m.type == cfg::MediaType::Fm) audio::playFm(m.freq);
  else audio::playUrls(m.urls);
  prefs.putInt("media", idx);
}

void stopMedia() {
  audio::stop();
  playingIdx = -1;
}

void clearMissed() {
  missed = Missed{};
  prefs.remove("mNum");
  prefs.remove("mAt");
  prefs.remove("mCnt");
}

void silence() {
  if (call.phase != Phase::Incoming || call.silenced) return;
  call.silenced = true;
  audio::ring(false, 0);
  render();
}

// Prawa kolumna: znaczenie zależy od ekranu, ale góra = tak, dół = nie/wstecz tam, gdzie to możliwe
void rightAction(int slot) {
  if (!ui::railVisible(slot)) return;
  ui::flashRail(slot);
  audio::sound(audio::Sound::Key);

  if (call.phase == Phase::Incoming) {
    if (slot == 0) {
      audio::ring(false, 0);
      modem::answer();
    } else if (slot == 1) {
      silence();
    } else {
      call.rejected = true;
      audio::ring(false, 0);
      modem::hangup();
    }
    return;
  }
  if (call.phase != Phase::None) {
    if (slot == 2) {
      call.hungUpHere = true;
      modem::hangup();
    }
    return;
  }

  int nC = C.contacts.size(), nM = C.mediaList.size();
  switch (scr) {
    case Screen::Home:
      if (missed.has) {
        if (slot == 0) {
          char num[24];
          strlcpy(num, missed.number, sizeof num);
          int k = C.contactIndexByNumber(num);
          clearMissed();
          startCall(num, k);
        } else if (slot == 2) {
          clearMissed();
          render();
        }
        return;
      }
      switch (C.home[slot].kind) {
        case cfg::SlotKind::Contact: {
          const cfg::Contact* k = C.contactById(C.home[slot].contactId);
          if (!k) return;
          confirmIdx = k - &C.contacts[0];
          confirmAt = millis();
          go(Screen::Confirm);  // jedno naciśnięcie z kieszeni nie dzwoni
          break;
        }
        case cfg::SlotKind::Media:
          if (audio::state().playing) {
            mediaIdx = playingIdx >= 0 ? playingIdx : mediaIdx;
          } else {
            playMedia(mediaIdx);
          }
          go(Screen::Media);
          break;
        case cfg::SlotKind::Weather:
          net::refreshWeather();
          go(Screen::Weather);
          break;
        default: break;
      }
      break;
    case Screen::Contacts:
      if (slot == 0 && nC) contactIdx = (contactIdx + nC - 1) % nC;
      if (slot == 2 && nC) contactIdx = (contactIdx + 1) % nC;
      if (slot == 1 && nC) {
        startCall(C.contacts[contactIdx].number.c_str(), contactIdx);
        return;
      }
      render();
      break;
    case Screen::Media: {
      bool playing = audio::state().src != audio::Src::None;
      if ((slot == 0 || slot == 2) && nM) {
        mediaIdx = slot == 0 ? (mediaIdx + nM - 1) % nM : (mediaIdx + 1) % nM;
        if (playing) playMedia(mediaIdx);  // jak pokrętło w radiu: przełącza od razu
      } else if (slot == 1) {
        if (playing && playingIdx == mediaIdx) stopMedia();
        else playMedia(mediaIdx);
      }
      render();
      break;
    }
    case Screen::Weather:
      if (slot == 2) go(Screen::Home);
      break;
    case Screen::Confirm:
      if (slot == 0) {
        const cfg::Contact* k = contactAt(confirmIdx);
        scr = Screen::Home;
        if (k) startCall(k->number.c_str(), confirmIdx);
      } else if (slot == 2) {
        go(Screen::Home);
      }
      break;
    case Screen::Unpaired: break;
    case Screen::Service:
      if (slot == 0) {
        net::startProvisioning();
        ui::toast("Sieć LUMI włączona na 5 minut");
      } else if (slot == 2) {
        go(haveConfig ? Screen::Home : Screen::Unpaired);
      }
      break;
  }
}

void shutdown() {
  esp_task_wdt_delete(nullptr);  // wyłączanie trwa kilka sekund - strażnik nie może przerwać
  ui::powerHold(false, 0, 0);
  ui::dismissNotice();
  ui::hideRails();
  ui::clearContent();
  lv_obj_t* l = ui::label(ui::content(), ui::fonts().title, ui::pal().text, "Do widzenia");
  lv_obj_center(l);
  audio::stop();
  modem::powerOff();
  uint32_t t = millis();
  while (millis() - t < 3500) {  // modem potrzebuje chwili na wyrejestrowanie z sieci
    display::tick();
    delay(10);
  }
  display::off();
  digitalWrite(pins::AMP_SD, LOW);
  while (digitalRead(pins::BTN_POWER) == LOW) delay(10);  // inaczej obudzi się od razu
  delay(200);
  rtc_gpio_pullup_en((gpio_num_t)pins::BTN_POWER);
  rtc_gpio_pulldown_dis((gpio_num_t)pins::BTN_POWER);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)pins::BTN_POWER, 0);
  esp_deep_sleep_start();
}

void onKey(Key k, Ev type) {
  bool wasOff = !screenOn;
  lastInput = millis();

  if (k == Key::Power) {
    if (type == Ev::KeyHoldStart) {
      powerHolding = true;
      powerHoldAt = millis();
      return;
    }
    if (type == Ev::KeyLong) {
      powerHolding = false;
      ui::powerHold(false, 0, 0);
      if (call.phase != Phase::None) return;
      if (C.allowPowerOff || !haveConfig) shutdown();
      wake();
      ui::toast("Wyłączanie zablokowała rodzina");
      return;
    }
    // krótkie naciśnięcie: wygaszenie / włączenie ekranu
    bool longish = powerHolding && millis() - powerHoldAt > 600;
    powerHolding = false;
    ui::powerHold(false, 0, 0);
    if (longish) return;  // puszczony w trakcie przytrzymania - tylko anuluj
    if (call.phase == Phase::Incoming || call.phase == Phase::Active) {
      wake();
      return;
    }
    if (screenOn) {
      screenOn = false;
      applyBacklight();
    } else {
      wake();
    }
    return;
  }

  // Wygaszony ekran: pierwsze naciśnięcie tylko budzi (nic się nie dzieje przypadkiem)
  if (wasOff && call.phase == Phase::None) {
    wake();
    return;
  }
  wake();

  if (ui::noticeVisible() && call.phase == Phase::None) {
    ui::dismissNotice();
    audio::sound(audio::Sound::Key);
    return;
  }

  bool right = k == Key::Top || k == Key::Mid || k == Key::Bottom;
  if (right) {
    int slot = (int)k;
    if (C.oneButton && k == Key::Mid) {
      if (type == Ev::KeyShort) {  // przejdź do następnej opcji
        for (int i = 1; i <= 3; i++) {
          int s = (highlight + i) % 3;
          if (ui::railVisible(s)) {
            highlight = s;
            break;
          }
        }
        ui::setHighlight(highlight);
        audio::sound(audio::Sound::Key);
        return;
      }
      if (type == Ev::KeyLong) {
        rightAction(highlight);
        return;
      }
      return;  // HoldStart
    }
    if (type == Ev::KeyShort) rightAction(slot);
    return;
  }
  if (k == Key::Home && type == Ev::KeyLong && call.phase == Phase::None) {
    audio::sound(audio::Sound::Confirm);
    go(Screen::Service);
    return;
  }
  if (type != Ev::KeyShort) return;

  // Dolny rząd: zawsze to samo, na każdym ekranie
  if (call.phase != Phase::None) {
    ui::toast(call.phase == Phase::Incoming ? "Najpierw odbierz albo odrzuć" : "Najpierw rozłącz rozmowę");
    audio::sound(audio::Sound::Error);
    return;
  }
  audio::sound(audio::Sound::Key);
  switch (k) {
    case Key::Home:
      ui::dismissNotice();
      go(haveConfig ? Screen::Home : Screen::Unpaired);
      break;
    case Key::Music:
      if (!audio::hasMedia()) {
        ui::toast("Radio nie jest podłączone w tym prototypie");
        break;
      }
      if (!haveConfig || C.lockMusic || !C.media) {
        ui::toast("Ten przycisk jest wyłączony");
        break;
      }
      if (playingIdx >= 0) mediaIdx = playingIdx;
      go(Screen::Media);
      break;
    case Key::Call:
      if (!haveConfig || C.lockCall || !C.calls) {
        ui::toast("Ten przycisk jest wyłączony");
        break;
      }
      go(Screen::Contacts);
      break;
    default: break;
  }
}

void onWheel(int d) {
  wake();
  if (call.phase == Phase::Active || call.phase == Phase::Outgoing) {
    callVolume = clampv<int>(callVolume + d, 1, 10);
    modem::setCallVolume(callVolume);
    prefs.putUChar("callVol", callVolume);  // senior ustawia raz, zostaje na kolejne rozmowy
    ui::volumeBar("Rozmowa", callVolume, 10);
    return;
  }
  if (call.phase == Phase::Incoming) {
    if (d < 0) silence();  // skręcenie w dół wycisza dzwonek
    return;
  }
  if (!audio::hasMedia()) {  // prototyp bez radia: rolka ustawia dzwonek
    ringOverride = clampv<int>(ringLevel() + d, 1, 10);
    modem::ringer(ringOverride);
    ui::volumeBar("Dzwonek", ringOverride, 10);
    return;
  }
  volume = clampv<int>(volume + d, 0, C.maxVolume);
  audio::setVolume(volume);
  ui::volumeBar("Głośność", volume, C.maxVolume);
}

void onIncoming(const char* num) {
  int k = C.contactIndexByNumber(num);
  if (C.onlyContacts && k < 0) {
    modem::hangup();
    Serial.println("[rozmowa] odrzucono numer spoza listy kontaktów");
    return;
  }
  if (call.phase != Phase::None) return;
  if (ui::noticeVisible()) {  // rozmowa ma pierwszeństwo, komunikat wróci po niej
    notices.push_front(shownNotice);
    ui::dismissNotice();
  }
  ui::powerHold(false, 0, 0);
  powerHolding = false;
  call = Call{};
  call.phase = Phase::Incoming;
  strlcpy(call.number, num, sizeof call.number);
  call.contact = k;
  wake();
  bool quiet = timeValid() && C.inQuietHours(minuteOfDay());
  // W godzinach ciszy dzwonek jest cichy, ale nigdy wyłączony - rodzina musi się dodzwonić
  audio::ring(true, quiet ? (ringLevel() < 3 ? ringLevel() : 3) : ringLevel());
  if (k >= 0 && C.contacts[k].autoAnswer) call.autoAt = millis() + C.autoAnswerSec * 1000UL;
  render();
}

void onEnded(bool answered, const char* text) {
  Phase was = call.phase;
  if (was == Phase::None) return;
  if (was == Phase::Incoming && !answered && !call.rejected) {
    if (missed.has && at::sameNumber(missed.number, call.number)) {
      missed.count++;
    } else {
      missed = Missed{};
      missed.has = true;
      strlcpy(missed.number, call.number, sizeof missed.number);
      missed.count = 1;
    }
    missed.at = time(nullptr);
    prefs.putString("mNum", missed.number);
    prefs.putLong64("mAt", missed.at);
    prefs.putInt("mCnt", missed.count);
  }
  if (was == Phase::Outgoing && !answered && !call.hungUpHere)
    ui::toast(!strcmp(text, "error") ? "Nie udało się połączyć" : "Nikt nie odebrał");
  if (was == Phase::Active && !call.hungUpHere) ui::toast("Rozmowa zakończona");
  audio::ring(false, 0);
  audio::callMode(false);
  call = Call{};
  scr = haveConfig ? Screen::Home : Screen::Unpaired;
  wake();
  render();
}

void announceChange() {
  if (C.changedBy.empty()) return;
  std::string t;
  if (!C.changeText.empty()) t = C.changedBy + " " + C.changeText;  // aplikacja zna rodzaj gramatyczny
  else t = "Ustawienia zmienione przez: " + C.changedBy;
  notices.push_back(t);
}

void applyConfig() {
  ui::setTheme(C.textSize, C.highContrast);
  input::configure(C.minPressMs, C.repeatIgnoreMs, C.oneButton);
  audio::setMaxVolume(C.maxVolume);
  audio::setKeyClick(C.keyClick);
  if (!audio::hasMedia()) modem::ringer(ringLevel());
  if (volume > C.maxVolume) {
    volume = C.maxVolume;
    audio::setVolume(volume);
  }
  net::Settings s;
  s.wifi = C.wifi;
  s.lat = C.lat;
  s.lon = C.lon;
  s.weather = C.weather;
  s.syncIntervalSec = C.syncIntervalSec;
  s.version = C.version;
  net::apply(s);
  applyBacklight();
}

bool loadFile(const char* path, cfg::Config& out) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  std::string s;
  s.resize(f.size());
  f.read(reinterpret_cast<uint8_t*>(&s[0]), s.size());
  f.close();
  std::string err;
  if (!cfg::parse(s.data(), s.size(), out, err)) {
    diag::error("[config] %s: %s", path, err.c_str());
    return false;
  }
  return true;
}

bool loadConfig() {
  cfg::Config n;
  if (!loadFile("/config.json", n) && !loadFile("/config.bak", n)) return false;
  C = std::move(n);
  haveConfig = true;
  return true;
}

void checkBattery() {
  uint8_t p = battery::percent();
  if (battery::charging() || p > 30) {
    batteryWarned = 100;
    return;
  }
  uint8_t level = p <= 10 ? 10 : (p <= 20 ? 20 : 100);
  if (level < batteryWarned) {
    batteryWarned = level;
    char b[80];
    snprintf(b, sizeof b, "Bateria słaba (%u%%). Podłącz ładowarkę.", p);
    notices.push_back(b);
  }
}

void tick() {
  uint32_t now = millis();
  if (now - lastTick < 100) return;
  lastTick = now;

  if (powerHolding && (C.allowPowerOff || !haveConfig) && now - powerHoldAt > 300)
    ui::powerHold(true, now - powerHoldAt, POWER_LONG_MS);

  // automatyczne odbieranie: sygnał, potem otwarcie rozmowy
  if (call.phase == Phase::Incoming && call.autoAt && (int32_t)(now - call.autoAt) >= 0) {
    if (call.autoStage == 0) {
      call.autoStage = 1;
      call.autoAt = now + 1500;
      audio::ring(false, 0);
      audio::sound(audio::Sound::AutoAnswer);
    } else {
      call.autoAt = 0;
      call.autoAnswered = true;
      modem::answer();
    }
  }

  if (scr == Screen::Confirm && call.phase == Phase::None && now - confirmAt > CONFIRM_TIMEOUT_MS) go(Screen::Home);

  if (screenOn && call.phase == Phase::None && now - lastInput > C.screenOffSec * 1000UL) {
    screenOn = false;
    applyBacklight();
    ui::dismissNotice();
    if (scr != Screen::Unpaired) scr = Screen::Home;  // po wybudzeniu zawsze zegar
    render();
  }

  if (!ui::noticeVisible() && call.phase == Phase::None && !notices.empty()) {
    wake();
    shownNotice = notices.front();
    notices.pop_front();
    ui::notice(IC_INFO, shownNotice.c_str());
    audio::sound(audio::Sound::Notice);
  }

  idleFlag = !screenOn && call.phase == Phase::None && !audio::state().playing;
  callFlag = call.phase != Phase::None;
  if (!healthyMarked && now > 180000) {  // 3 minuty pracy: nowa wersja oprogramowania jest zdrowa
    healthyMarked = true;
    ota::markHealthy();
  }

  if (now - lastSecond >= 1000) {
    lastSecond = now;
    updateLive();
    checkBattery();
    tm l = now_tm();
    if (l.tm_min != lastMinute) {
      lastMinute = l.tm_min;
      if (scr == Screen::Home && call.phase == Phase::None) render();
    }
  }
}

void handle(const Event& e) {
  switch (e.type) {
    case Ev::KeyShort:
    case Ev::KeyHoldStart:
    case Ev::KeyLong: onKey((Key)e.a, e.type); break;
    case Ev::Wheel: onWheel(e.a); break;
    case Ev::ModemReady:
      modemReady = true;
      modemFailed = false;
      simOk = e.a == 1;
      if (call.phase == Phase::None) render();
      break;
    case Ev::ModemFail:
      modemFailed = true;
      if (call.phase == Phase::None) render();
      break;
    case Ev::NetTime:
    case Ev::WeatherUpdated:
      if (call.phase == Phase::None && (scr == Screen::Home || scr == Screen::Weather)) render();
      break;
    case Ev::CallIncoming: onIncoming(e.text); break;
    case Ev::CallDialing: break;
    case Ev::CallActive:
      if (call.phase == Phase::Incoming || call.phase == Phase::Outgoing) {
        call.phase = Phase::Active;
        call.startedAt = millis();
        audio::callMode(true);
        modem::setCallVolume(callVolume);
        render();
      }
      break;
    case Ev::CallEnded: onEnded(e.a == 1, e.text); break;
    case Ev::ConfigUpdated: reloadConfig(true); break;
    case Ev::CloudState:
      if (call.phase == Phase::None && (scr == Screen::Unpaired || scr == Screen::Service)) render();
      break;
    case Ev::MediaState:
      applyBacklight();  // FM -> podświetlenie bez PWM
      if (call.phase == Phase::None && (scr == Screen::Media || scr == Screen::Home)) render();
      break;
  }
}

}  // namespace

// ---------- interfejs publiczny ----------

void begin() {
  identity::begin();
  strlcpy(devId, identity::id(), sizeof devId);
  prefs.begin("lumi", false);
  callVolume = prefs.getUChar("callVol", 7);
  ui::begin();
  bool ok = loadConfig();
  if (!ok) Serial.println("[config] brak konfiguracji - ekran parowania");
  volume = C.startVolume;

  if (prefs.isKey("mNum")) {
    missed.has = true;
    strlcpy(missed.number, prefs.getString("mNum", "").c_str(), sizeof missed.number);
    missed.at = prefs.getLong64("mAt", 0);
    missed.count = prefs.getInt("mCnt", 1);
  }
  mediaIdx = prefs.getInt("media", 0);

  audio::begin(volume);
  modem::begin();
  net::begin();
  applyConfig();
  scr = ok ? Screen::Home : Screen::Unpaired;
  lastInput = millis();
  // zasada 5: użytkownik wie, że coś się zmieniło
  if (ota::updatedThisBoot()) notices.push_back("LUMI ma nową wersję oprogramowania");
  render();
}

void loop() {
  Event e;
  while (bus::receive(e, 0)) handle(e);
  tick();
}

const cfg::Config& config() { return C; }
bool configured() { return haveConfig; }
const char* deviceId() { return devId; }

bool reloadConfig(bool announce) {
  int oldVersion = haveConfig ? C.version : -1;
  if (!loadConfig()) return false;
  applyConfig();
  if (announce && C.version > oldVersion) announceChange();
  if (scr == Screen::Unpaired) scr = Screen::Home;
  if (call.phase == Phase::None) render();
  return true;
}

void showNotice(const char* text) { notices.push_back(text); }

bool idleForUpdate() { return idleFlag; }
bool inCall() { return callFlag; }

}  // namespace app
