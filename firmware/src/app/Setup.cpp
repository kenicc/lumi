#include "Setup.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <string>
#include <vector>

#include "App.h"
#include "State.h"
#include "../core/AtParse.h"
#include "../core/LocalEdit.h"
#include "../core/Pairing.h"
#include "../modem/Modem.h"
#include "../net/Identity.h"
#include "../net/Net.h"
#include "../ui/Icons.h"
#include "../ui/Ui.h"

LV_FONT_DECLARE(lumi_b20);
LV_FONT_DECLARE(lumi_b24);
LV_FONT_DECLARE(lumi_b28);
LV_FONT_DECLARE(lumi_b48);
LV_FONT_DECLARE(lumi_r20);

namespace app {
namespace detail {
void qrBox(lv_obj_t* parent, const std::string& payload, int size);  // Screens.cpp
}
}  // namespace app

namespace localsetup {

using namespace app::detail;

namespace {

enum class Page : uint8_t { Main, Wifi, WifiPhone, WifiPass, Contacts, Remove, Number, NamePick, NameType, Sim, TextSize, Ring, Pair, Info };

Page page = Page::Main;
int sel = 0;
int mainSel = 0, contactsSel = 0;  // powrót na tę samą pozycję

// wpisywanie tekstu rolką
std::string entry;
int charIdx = 0;
std::string ssid, number;  // sieć, do której wpisujemy hasło; numer nowej osoby
int ringLevel = 8;
bool simLoading = false, scanning = false;

const char* const NUMBER_CHARS[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+"};
const char* const NAME_CHARS[] = {"a", "ą", "b", "c", "ć", "d", "e", "ę", "f", "g", "h", "i", "j", "k", "l", "ł", "m", "n", "ń",
                                  "o", "ó", "p", "r", "s", "ś", "t", "u", "w", "y", "z", "ź", "ż", "q", "v", "x", " ", "-"};
const char* const NAME_UPPER[] = {"A", "Ą", "B", "C", "Ć", "D", "E", "Ę", "F", "G", "H", "I", "J", "K", "L", "Ł", "M", "N", "Ń",
                                  "O", "Ó", "P", "R", "S", "Ś", "T", "U", "W", "Y", "Z", "Ź", "Ż", "Q", "V", "X", " ", "-"};
// hasło: małe litery, cyfry, wielkie, znaki - kolejność według tego, czego w hasłach najwięcej
const char PASS_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%&*-_.+=?/:;,()[]{}<>'\"^~|\\` ";

const char* const PRESETS[] = {"Córka", "Syn", "Wnuczka", "Wnuk", "Żona", "Mąż", "Siostra", "Brat", "Sąsiadka", "Sąsiad", "Lekarz", "Przychodnia"};
constexpr int NPRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);
const char* const SIZES[] = {"Normalny", "Duży", "Bardzo duży"};

template <typename T, size_t N>
constexpr int count(T (&)[N]) {
  return (int)N;
}

int charCount() {
  switch (page) {
    case Page::Number: return count(NUMBER_CHARS);
    case Page::NameType: return count(NAME_CHARS);
    default: return (int)sizeof(PASS_CHARS) - 1;
  }
}

// Wielka litera na początku imienia i po spacji albo myślniku
bool upperNow() { return entry.empty() || entry.back() == ' ' || entry.back() == '-'; }

std::string charAt(int i) {
  switch (page) {
    case Page::Number: return NUMBER_CHARS[i];
    case Page::NameType: return upperNow() ? NAME_UPPER[i] : NAME_CHARS[i];
    default: return std::string(1, PASS_CHARS[i]);
  }
}

void eraseLastChar(std::string& s) {
  if (s.empty()) return;
  size_t i = s.size() - 1;
  while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;  // cały znak UTF-8
  s.erase(i);
}

// 1 osoba, 2-4 osoby, 5+ osób (ale 22 osoby); biernik: "dodano 1 osobę"
std::string people(int n, bool accusative = false) {
  int d = n % 10, t = n % 100;
  const char* w = n == 1 ? (accusative ? "osobę" : "osoba") : (d >= 2 && d <= 4 && (t < 12 || t > 14)) ? "osoby" : "osób";
  return std::to_string(n) + " " + w;
}

struct Item {
  const char* icon;
  std::string text, sub;
};

void go(Page p, int s = 0) {
  page = p;
  sel = s;
}

std::vector<std::string> scanned() { return net::scanResults(); }

bool hasNumber(const std::string& num) {
  for (const auto& k : C.contacts)
    if (at::sameNumber(k.number.c_str(), num.c_str())) return true;
  return false;
}

std::string readConfig() {
  if (!haveConfig) return "";
  File f = LittleFS.open("/config.json", "r");
  if (!f) return "";
  std::string s;
  s.resize(f.size());
  f.read(reinterpret_cast<uint8_t*>(&s[0]), s.size());
  f.close();
  return s;
}

// Zapisuje zmianę na LUMI, a jeśli LUMI jest w aplikacji - wysyła ją też rodzinie (serwer nakłada
// ją na aktualne ustawienia, więc równoległa zmiana w aplikacji nie przepada).
bool applyOp(const local::Op& op, std::string& err) {
  std::string out;
  if (!local::apply(readConfig(), op, out, err)) return false;
  if (!net::saveConfigFile(out.c_str(), out.size())) return err = "zapis", false;
  int before = haveConfig ? C.version : -1;
  app::reloadConfig(false);
  if (before > 0) net::queueEvent(local::eventJson(op));
  return true;
}

std::string newId() {
  char b[16];
  snprintf(b, sizeof b, "d%08lx", (unsigned long)(millis() * 2654435761UL ^ (uint32_t)C.contacts.size()));
  return b;
}

bool addContact(const std::string& name, const std::string& num) {
  local::Op op;
  op.kind = local::Kind::AddContact;
  op.id = newId();
  op.name = name.size() ? name : "Osoba";
  op.number = num;
  std::string err;
  if (applyOp(op, err)) return true;
  if (err == "ten numer juz jest") ui::toast("Ten numer już jest w kontaktach");
  else if (err == "za duzo kontaktow") ui::toast("Najwyżej 30 kontaktów");
  else if (err == "zle imie") ui::toast("Imię może mieć do 24 znaków");
  else ui::toast("Nie udało się zapisać");
  return false;
}

std::vector<Item> items() {
  std::vector<Item> v;
  switch (page) {
    case Page::Main: {
      std::string w = net::wifiConnected() ? net::wifiSsid() : "nie połączono";
      std::string k = people((int)C.contacts.size());
      char r[16];
      snprintf(r, sizeof r, "%u z 10", (unsigned)(haveConfig ? C.ringVolume : 8));
      v.push_back({IC_WIFI, "Wi-Fi", w});
      v.push_back({IC_USER, "Kontakty", k});
      v.push_back({IC_INFO, "Wielkość tekstu", SIZES[haveConfig ? C.textSize % 3 : 0]});
      v.push_back({IC_BELL, "Dzwonek", r});
      v.push_back({IC_GLOBE, "Aplikacja rodziny", haveConfig && C.version > 0 ? "połączona" : "dodaj telefonem"});
      v.push_back({IC_INFO, "Informacje", ""});
      break;
    }
    case Page::Wifi: {
      std::string cur = net::wifiConnected() ? net::wifiSsid() : "";
      if (scanning) v.push_back({IC_WIFI, "Szukam sieci...", ""});
      for (const auto& s : scanned()) v.push_back({IC_WIFI, s, s == cur ? "połączono" : ""});
      if (!scanning) v.push_back({IC_WIFI, "Szukaj jeszcze raz", ""});
      v.push_back({IC_PHONE, "Ustaw telefonem", "kod QR"});
      break;
    }
    case Page::Contacts:
      v.push_back({IC_USER, "+ Dodaj osobę", "wpisz numer"});
      v.push_back({IC_SIGNAL, "Z karty SIM", "kontakty z telefonu"});
      for (const auto& k : C.contacts) {
        char n[32];
        at::formatNumber(k.number.c_str(), n, sizeof n);
        v.push_back({IC_USER, k.name, n});
      }
      break;
    case Page::Remove:
      v.push_back({IC_CLOSE, "Nie usuwaj", ""});
      v.push_back({IC_CHECK, "Usuń", ""});
      break;
    case Page::NamePick:
      v.push_back({IC_USER, "Wpisz imię", "literami"});
      for (int i = 0; i < NPRESETS; i++) v.push_back({IC_USER, PRESETS[i], ""});
      break;
    case Page::Sim: {
      if (simLoading) {
        v.push_back({IC_SIGNAL, "Czytam kartę SIM...", ""});
        break;
      }
      auto list = modem::simContacts();
      if (list.empty()) {
        v.push_back({IC_WARN, "Brak kontaktów", modemReady && simOk ? "karta SIM jest pusta" : "nie widzę karty SIM"});
        break;
      }
      int fresh = 0;
      for (const auto& k : list) fresh += !hasNumber(k.number);
      v.push_back({IC_CHECK, "Dodaj nowe", fresh ? people(fresh) + " z karty" : "wszystkie już są"});
      for (const auto& k : list) {
        char n[32];
        at::formatNumber(k.number.c_str(), n, sizeof n);
        v.push_back({IC_USER, k.name, hasNumber(k.number) ? std::string("jest - ") + n : n});
      }
      break;
    }
    case Page::TextSize:
      for (int i = 0; i < 3; i++) v.push_back({i == (haveConfig ? C.textSize : 0) ? IC_CHECK : IC_INFO, SIZES[i], ""});
      break;
    default: break;
  }
  return v;
}

bool isList() {
  return page == Page::Main || page == Page::Wifi || page == Page::Contacts || page == Page::Remove ||
         page == Page::NamePick || page == Page::Sim || page == Page::TextSize;
}

bool isEntry() { return page == Page::Number || page == Page::NameType || page == Page::WifiPass; }

const char* title() {
  switch (page) {
    case Page::Main: return "Ustawienia";
    case Page::Wifi: return "Wi-Fi";
    case Page::WifiPhone: return "Wi-Fi z telefonu";
    case Page::WifiPass: return "Hasło Wi-Fi";
    case Page::Contacts: return "Kontakty";
    case Page::Remove: return "Usunąć?";
    case Page::Number: return "Numer telefonu";
    case Page::NamePick: return "Kto to?";
    case Page::NameType: return "Imię";
    case Page::Sim: return "Karta SIM";
    case Page::TextSize: return "Wielkość tekstu";
    case Page::Ring: return "Dzwonek";
    case Page::Pair: return "Aplikacja rodziny";
    case Page::Info: return "Informacje";
  }
  return "";
}

// jedna linia z wielokropkiem (bez stałej wysokości LVGL zawija zamiast skracać)
void oneLine(lv_obj_t* l, const lv_font_t* f, int w) {
  lv_obj_set_size(l, w, lv_font_get_line_height(f));
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
}

void renderList(lv_obj_t* col) {
  auto& P = ui::pal();
  auto v = items();
  if (sel >= (int)v.size()) sel = (int)v.size() - 1;
  if (sel < 0) sel = 0;
  // okno 3 pozycji wokół wybranej
  int first = sel - 1;
  if (first > (int)v.size() - 3) first = (int)v.size() - 3;
  if (first < 0) first = 0;
  int w = ui::contentInnerW() - 2 * ui::TILE_PAD - 4;
  for (int i = first; i < (int)v.size() && i < first + 3; i++) {
    // wybrana pozycja duża i z opisem, pozostałe skromniej - trzy mieszczą się zawsze
    bool on = i == sel;
    lv_obj_t* t = ui::tile(col);
    lv_obj_set_width(t, lv_pct(100));
    if (on) {
      lv_obj_set_style_border_width(t, 3, 0);
      lv_obj_set_style_border_color(t, P.accent, 0);
    }
    const lv_font_t* f = on && ui::fits(v[i].text.c_str(), &lumi_b28, w - 8) ? &lumi_b28 : &lumi_b24;  // ramka wybranej
    oneLine(ui::label(t, f, on ? P.text : P.dim, v[i].text.c_str()), f, w);
    if (on && !v[i].sub.empty()) oneLine(ui::label(t, &lumi_r20, P.dim, v[i].sub.c_str()), &lumi_r20, w);
  }
  // ile jeszcze w dół: senior widzi, że lista się nie kończy
  if (first + 3 < (int)v.size()) {
    char more[24];
    snprintf(more, sizeof more, IC_DOWN " jeszcze %d", (int)v.size() - first - 3);
    ui::label(col, &lumi_b20, P.dim, more);
  }
  ui::setRail(0, IC_UP, "Wyżej", ui::Tone::Neutral);
  ui::setRail(1, IC_CHECK, "Wybierz", ui::Tone::Yes);
  ui::setRail(2, IC_DOWN, "Niżej", ui::Tone::Neutral);
}

void renderEntry(lv_obj_t* col) {
  auto& P = ui::pal();
  int w = ui::contentInnerW();
  const std::string& shown = entry;  // hasło też widać: senior wpisuje je sam, na swoim urządzeniu
  // koniec tekstu, gdy się nie mieści - widać to, co właśnie wpisujemy
  std::string tail = shown + "_";
  while (local::utf8Len(tail) > 1 && !ui::fits(tail.c_str(), &lumi_b28, w)) {
    size_t i = 1;
    while (i < tail.size() && ((unsigned char)tail[i] & 0xC0) == 0x80) i++;
    tail.erase(0, i);
  }
  lv_obj_t* t = ui::tile(col);
  lv_obj_set_width(t, lv_pct(100));
  ui::label(t, &lumi_b28, P.text, tail.c_str());
  // bieżący znak duży, obok poprzedni i następny
  int n = charCount();
  auto disp = [&](int i) {
    std::string c = charAt((i % n + n) % n);
    return c == " " ? std::string("spacja") : c;
  };
  lv_obj_t* r = lv_obj_create(col);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 12, 0);
  ui::label(r, &lumi_b24, P.dim, disp(charIdx - 1).c_str());
  lv_obj_t* cur = ui::tile(r);
  lv_obj_set_flex_grow(cur, 0);
  lv_obj_set_size(cur, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(cur, 14, 0);
  lv_obj_set_style_border_width(cur, 3, 0);
  lv_obj_set_style_border_color(cur, P.accent, 0);
  ui::label(cur, &lumi_b48, P.text, disp(charIdx).c_str());
  ui::label(r, &lumi_b24, P.dim, disp(charIdx + 1).c_str());
  ui::label(col, &lumi_b20, P.dim, IC_HOME " usuń   " IC_PHONE " gotowe");
  ui::setRail(0, IC_UP, "Zmień", ui::Tone::Neutral);
  ui::setRail(1, IC_CHECK, "Dodaj", ui::Tone::Yes);
  ui::setRail(2, IC_DOWN, "Zmień", ui::Tone::Neutral);
}

void startEntry(Page p, const std::string& initial = "") {
  go(p);
  entry = initial;
  charIdx = 0;
}

void openPage(Page p) {
  switch (p) {
    case Page::Wifi:
      scanning = true;
      net::requestScan();
      go(Page::Wifi);
      break;
    case Page::Sim:
      simLoading = true;
      modem::requestSimBook();
      go(Page::Sim);
      break;
    case Page::Ring:
      ringLevel = haveConfig ? C.ringVolume : 8;
      if (ringLevel < 1) ringLevel = 1;
      go(Page::Ring);
      break;
    default: go(p); break;
  }
}

void back() {
  switch (page) {
    case Page::Wifi:
    case Page::Contacts:
    case Page::TextSize:
    case Page::Ring:
    case Page::Pair:
    case Page::Info: go(Page::Main, mainSel); break;
    case Page::WifiPhone:
    case Page::WifiPass: openPage(Page::Wifi); break;
    case Page::Remove:
    case Page::Number:
    case Page::Sim: go(Page::Contacts, contactsSel); break;
    case Page::NamePick: startEntry(Page::Number, number); break;
    case Page::NameType: go(Page::NamePick); break;
    case Page::Main: break;
  }
}

void choose() {
  auto v = items();
  if (isList() && (sel < 0 || sel >= (int)v.size())) return;
  switch (page) {
    case Page::Main:
      mainSel = sel;
      openPage(sel == 0 ? Page::Wifi : sel == 1 ? Page::Contacts : sel == 2 ? Page::TextSize : sel == 3 ? Page::Ring : sel == 4 ? Page::Pair : Page::Info);
      if (page == Page::TextSize) sel = haveConfig ? C.textSize : 0;
      break;
    case Page::Wifi: {
      auto nets = scanned();
      int i = sel - (scanning ? 1 : 0);
      if (scanning && sel == 0) break;
      if (i >= 0 && i < (int)nets.size()) {
        ssid = nets[i];
        startEntry(Page::WifiPass);
      } else if (!scanning && i == (int)nets.size()) {
        openPage(Page::Wifi);
      } else {
        net::startProvisioning();
        go(Page::WifiPhone);
      }
      break;
    }
    case Page::Contacts:
      contactsSel = sel;
      if (sel == 0) startEntry(Page::Number);
      else if (sel == 1) openPage(Page::Sim);
      else go(Page::Remove);
      break;
    case Page::Remove:
      if (sel == 1 && contactsSel >= 2 && contactsSel - 2 < (int)C.contacts.size()) {
        local::Op op;
        op.kind = local::Kind::RemoveContact;
        op.id = C.contacts[contactsSel - 2].id;
        std::string name = C.contacts[contactsSel - 2].name, err;
        if (applyOp(op, err)) ui::toast(("Usunięto: " + name).c_str());
        else ui::toast("Nie udało się usunąć");
        contactsSel = 0;
      }
      go(Page::Contacts, contactsSel);
      break;
    case Page::NamePick:
      if (sel == 0) {
        startEntry(Page::NameType);
      } else if (addContact(PRESETS[sel - 1], number)) {
        ui::toast((std::string("Dodano: ") + PRESETS[sel - 1]).c_str());
        go(Page::Contacts, 1 + (int)C.contacts.size());
      }
      break;
    case Page::Sim: {
      auto list = modem::simContacts();
      if (list.empty()) break;
      if (sel == 0) {
        int added = 0;
        for (const auto& k : list)
          if (!hasNumber(k.number) && C.contacts.size() < local::MAX_CONTACTS && addContact(k.name, k.number)) added++;
        ui::toast(("Dodano " + people(added, true)).c_str());
        go(Page::Contacts, 2);
      } else if (sel - 1 < (int)list.size()) {
        const auto& k = list[sel - 1];
        if (hasNumber(k.number)) ui::toast("Ta osoba już jest w kontaktach");
        else if (addContact(k.name, k.number)) ui::toast(("Dodano: " + k.name).c_str());
      }
      break;
    }
    case Page::TextSize: {
      local::Op op;
      op.kind = local::Kind::TextSize;
      op.value = sel;
      std::string err;
      if (applyOp(op, err)) ui::toast("Zapisano");
      break;
    }
    default: break;
  }
}

// Gotowe (zielony Dzwoń) na stronach z wpisywaniem i suwakiem
void done() {
  switch (page) {
    case Page::Number: {
      std::string n = local::normalizeNumber(entry);
      if (n.empty()) {
        ui::toast("To nie wygląda na numer");
        return;
      }
      if (hasNumber(n)) {
        ui::toast("Ten numer już jest w kontaktach");
        return;
      }
      number = n;
      go(Page::NamePick);
      break;
    }
    case Page::NameType: {
      std::string name = entry;
      while (!name.empty() && (name.back() == ' ' || name.back() == '-')) name.pop_back();
      if (name.empty()) {
        ui::toast("Wpisz imię");
        return;
      }
      if (addContact(name, number)) {
        ui::toast(("Dodano: " + name).c_str());
        go(Page::Contacts, 1 + (int)C.contacts.size());
      }
      break;
    }
    case Page::WifiPass:
      identity::addWifi(ssid, entry);  // hasło zostaje w LUMI - nie idzie na serwer
      net::networksChanged();
      ui::toast(("Łączę z " + ssid).c_str());
      go(Page::Main, mainSel);
      break;
    case Page::Ring: {
      local::Op op;
      op.kind = local::Kind::RingVolume;
      op.value = ringLevel;
      std::string err;
      if (applyOp(op, err)) ui::toast("Zapisano");
      go(Page::Main, mainSel);
      break;
    }
    default: choose(); break;
  }
}

void move(int d) {
  if (isList()) {
    // lista zatrzymuje się na końcach (przeskok z ostatniej na pierwszą dezorientuje)
    int n = (int)items().size();
    sel = app::detail::clampv(sel + d, 0, n ? n - 1 : 0);
  } else if (isEntry()) {
    int n = charCount();
    charIdx = ((charIdx + d) % n + n) % n;
  } else if (page == Page::Ring) {
    ringLevel = app::detail::clampv(ringLevel + d, 1, 10);
    modem::ringer(ringLevel);
  }
}

}  // namespace

void open() {
  go(Page::Main, 0);
  mainSel = 0;
}

void render() {
  auto& P = ui::pal();
  auto& F = ui::fonts();
  lv_obj_t* col = ui::column(ui::content(), 4);
  ui::label(col, &lumi_b20, P.accent, title());
  ui::hideRails();
  if (isList()) {
    renderList(col);
  } else if (isEntry()) {
    if (page == Page::WifiPass) ui::label(col, &lumi_r20, P.dim, ssid.c_str());
    renderEntry(col);
  } else if (page == Page::Ring) {
    char b[8];
    snprintf(b, sizeof b, "%d", ringLevel);
    ui::label(col, F.huge, P.text, b);
    ui::label(col, &lumi_r20, P.dim, "z 10");
    ui::setRail(0, IC_UP, "Głośniej", ui::Tone::Neutral);
    ui::setRail(1, IC_CHECK, "Zapisz", ui::Tone::Yes);
    ui::setRail(2, IC_DOWN, "Ciszej", ui::Tone::Neutral);
  } else if (page == Page::WifiPhone) {
    qrBox(col, pairing::wifiQr(identity::id(), identity::apPassword()), 120);
    ui::label(col, &lumi_r20, P.text, "potem 192.168.4.1");
    ui::setRail(2, IC_BACK, "Wróć", ui::Tone::Neutral);
  } else if (page == Page::Pair) {
    net::Cloud c = net::cloud();
    if (haveConfig && C.version > 0 && c != net::Cloud::Unclaimed) {
      ui::label(col, F.body, P.text, "LUMI jest już w aplikacji rodziny", true);
    } else if (!net::wifiConnected()) {
      ui::label(col, F.body, P.text, "Najpierw połącz LUMI z Wi-Fi", true);
    } else if (c == net::Cloud::Unclaimed) {
      qrBox(col, pairing::pairUrl(identity::server(), identity::id(), identity::pairCode()), 120);
      char code[24];
      snprintf(code, sizeof code, "Kod: %s", identity::pairCode());
      ui::label(col, &lumi_r20, P.text, code);
    } else {
      ui::label(col, F.body, P.text, "Łączę z serwerem...", true);
      ui::label(col, &lumi_r20, P.dim, net::cloudName(c), true);
    }
    ui::setRail(2, IC_BACK, "Wróć", ui::Tone::Neutral);
  } else if (page == Page::Info) {
    ui::label(col, F.small, P.text, identity::id());
    std::string w = net::wifiConnected() ? "Wi-Fi: " + net::wifiSsid() : "Wi-Fi: brak";
    ui::label(col, F.small, P.text, w.c_str(), true);
    ui::label(col, F.small, P.text, net::cloudName(net::cloud()), true);
    modem::Status m = modem::status();
    char b[64];
    snprintf(b, sizeof b, "%s, zasięg %d/4", modem::vendorName(m.vendor), m.bars);
    ui::label(col, F.small, P.dim, b, true);
    ui::setRail(2, IC_BACK, "Wróć", ui::Tone::Neutral);
  }
}

bool onKey(Key k, Ev type) {
  if (k == Key::Home && type == Ev::KeyLong) return true;  // przytrzymany Dom: do zegara
  if (type != Ev::KeyShort) return false;
  switch (k) {
    case Key::Top: move(-1); break;
    case Key::Bottom:
      if (!isList() && !isEntry() && page != Page::Ring) back();  // ekrany z samym "Wróć"
      else move(1);
      break;
    case Key::Mid:
      if (isEntry()) {
        if (local::utf8Len(entry) < (page == Page::WifiPass ? 63u : page == Page::Number ? 20u : local::MAX_NAME))
          entry += charAt(charIdx);
      } else if (page == Page::Ring) {
        done();
      } else {
        choose();
      }
      break;
    case Key::Call:
      if (isEntry() || page == Page::Ring) done();
      else choose();
      break;
    case Key::Home:
      if (isEntry() && !entry.empty()) {
        eraseLastChar(entry);
        break;
      }
      if (page == Page::Main) return true;
      back();
      break;
    default: return false;
  }
  return false;
}

void onWheel(int d) { move(d > 0 ? 1 : -1); }

void onEvent(Ev e) {
  if (e == Ev::WifiScan) scanning = false;
  if (e == Ev::SimBook) simLoading = false;
}

}  // namespace localsetup
