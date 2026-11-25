#include "app/SelfTest.h"

#include <Arduino.h>

#include <string>

#include "audio/AudioOut.h"
#include "hw/Battery.h"
#include "hw/Board.h"
#include "modem/Modem.h"
#include "net/Net.h"
#include "ui/Icons.h"
#include "ui/Ui.h"

LV_FONT_DECLARE(lumi_b20);
LV_FONT_DECLARE(lumi_r20);
LV_FONT_DECLARE(lumi_b28);

namespace selftest {

namespace {

enum class Step : uint8_t { Screen, Buttons, Wheel, Modem, Sound, Battery, Network, Summary };
enum class Res : uint8_t { None, Pass, Fail };

constexpr int STEPS = 7;  // bez podsumowania
const char* const kStepName[STEPS] = {"Ekran", "Przyciski", "Rolka", "Modem", "Dźwięk", "Bateria", "Sieć"};

struct Btn {
  Key key;
  const char* name;
  int gpio;
};
const Btn kButtons[] = {{Key::Top, "Górny", pins::BTN_TOP},       {Key::Mid, "Środkowy", pins::BTN_MID},
                        {Key::Bottom, "Dolny", pins::BTN_BOTTOM}, {Key::Music, "Muzyka", pins::BTN_MUSIC},
                        {Key::Home, "Home", pins::BTN_HOME},      {Key::Call, "Dzwoń", pins::BTN_CALL},
                        {Key::Power, "Włącznik", pins::BTN_POWER}};
constexpr int NBTN = sizeof(kButtons) / sizeof(kButtons[0]);

bool on = false;
Step step = Step::Screen;
Res results[STEPS];
std::string notes[STEPS];
uint32_t stepAt = 0, lastDraw = 0, lastBeep = 0;
bool pressed[NBTN];
int wheelRight = 0, wheelLeft = 0;
bool wheelPhaseLeft = false;
lv_obj_t* liveLbl = nullptr;

uint32_t elapsed() { return millis() - stepAt; }

lv_obj_t* text(const lv_font_t* f, lv_color_t c, const char* s) { return ui::label(ui::content(), f, c, s, true); }

void header(const char* title, const char* hint) {
  auto& P = ui::pal();
  lv_obj_t* col = ui::column(ui::content(), 4);
  lv_obj_set_size(col, lv_pct(100), LV_SIZE_CONTENT);
  char b[64];
  snprintf(b, sizeof b, "Test %d z %d: %s", (int)step + 1, STEPS, title);
  ui::label(col, &lumi_b20, P.accent, b);
  if (hint) ui::label(col, &lumi_r20, P.text, hint, true);
}

void draw() {
  auto& P = ui::pal();
  ui::clearContent();
  ui::setContentWide(true);
  ui::hideRails();
  liveLbl = nullptr;
  lastDraw = millis();
  switch (step) {
    case Step::Screen: {
      // pasy kolorów: od razu widać negatyw albo zamieniony czerwony z niebieskim
      const uint32_t cols[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
      for (int i = 0; i < 4; i++) {
        lv_obj_t* r = lv_obj_create(ui::content());
        lv_obj_remove_style_all(r);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(r, lv_color_hex(cols[i]), 0);
        lv_obj_set_size(r, 76, 90);
        lv_obj_set_pos(r, i * 77, 0);
      }
      lv_obj_t* l = text(&lumi_b20, P.text, "Kolejno: czerwony, zielony, niebieski, biały?\nGórny = tak, dolny = nie");
      lv_obj_set_pos(l, 0, 100);
      break;
    }
    case Step::Buttons: {
      header("przyciski", "Naciśnij każdy przycisk po kolei.");
      for (int i = 0; i < NBTN; i++) {
        char b[40];
        snprintf(b, sizeof b, "%s %s (GPIO%d)", pressed[i] ? IC_CHECK : "-", kButtons[i].name, kButtons[i].gpio);
        lv_obj_t* l = ui::label(ui::content(), &lumi_b20, pressed[i] ? P.yes : P.dim, b);
        lv_obj_set_pos(l, 0, 56 + i * 25);
      }
      break;
    }
    case Step::Wheel:
      header("rolka głośności", wheelPhaseLeft ? "Teraz pokręć rolką w LEWO (ciszej)." : "Pokręć rolką w PRAWO (głośniej).");
      liveLbl = text(&lumi_b28, P.text, "");
      lv_obj_align(liveLbl, LV_ALIGN_BOTTOM_LEFT, 0, -20);
      break;
    case Step::Modem:
      header("modem i karta SIM", "Czekam na modem i sieć (do 2 minut). Górny = pomiń.");
      liveLbl = text(&lumi_r20, P.text, "");
      lv_obj_align(liveLbl, LV_ALIGN_BOTTOM_LEFT, 0, -10);
      break;
    case Step::Sound:
      header("dźwięk", "Słyszysz sygnał co kilka sekund?\nGórny = tak, dolny = nie");
      break;
    case Step::Battery:
      header("bateria", "Górny = dalej");
      liveLbl = text(&lumi_b28, P.text, "");
      lv_obj_align(liveLbl, LV_ALIGN_BOTTOM_LEFT, 0, -20);
      break;
    case Step::Network:
      header("Wi-Fi i serwer", "Informacyjnie - zależy od konfiguracji. Górny = dalej");
      liveLbl = text(&lumi_r20, P.text, "");
      lv_obj_align(liveLbl, LV_ALIGN_BOTTOM_LEFT, 0, -20);
      break;
    case Step::Summary: {
      lv_obj_t* col = ui::column(ui::content(), 2);
      ui::label(col, &lumi_b20, P.accent, "Wynik (szczegóły w konsoli USB)");
      for (int i = 0; i < STEPS; i++) {
        char b[96];
        bool ok = results[i] == Res::Pass;
        // na ekranie krótka przyczyna, pełna podpowiedź (które piny sprawdzić) jest w konsoli
        std::string shortNote = notes[i].substr(0, notes[i].find(':'));
        snprintf(b, sizeof b, "%s %s%s%s", ok ? IC_CHECK : IC_CLOSE, kStepName[i], shortNote.empty() ? "" : ": ",
                 shortNote.c_str());
        ui::label(col, &lumi_b20, ok ? P.yes : P.no, b, true);  // ta czcionka ma ikony
      }
      break;
    }
  }
}

void finish(Res r, const std::string& note = "") {
  int i = (int)step;
  results[i] = r;
  notes[i] = note;
  Serial.printf("[test] %s: %s%s%s\n", kStepName[i], r == Res::Pass ? "OK" : "BLAD", note.empty() ? "" : " - ",
                note.c_str());
  step = (Step)(i + 1);
  stepAt = millis();
  if (step == Step::Summary) {
    int bad = 0;
    for (auto x : results) bad += x != Res::Pass;
    Serial.printf("[test] koniec: %d z %d w porzadku\n", STEPS - bad, STEPS);
  }
  draw();
}

void updateLive() {
  if (!liveLbl) return;
  char b[160];
  switch (step) {
    case Step::Wheel:
      snprintf(b, sizeof b, "w prawo: %d   w lewo: %d", wheelRight, wheelLeft);
      break;
    case Step::Modem: {
      modem::Status m = modem::status();
      snprintf(b, sizeof b, "modem: %s %s\nSIM: %s, sieć: %s, zasięg %d/4\nczekam %u s", m.ready ? modem::vendorName(m.vendor) : "brak odpowiedzi",
               m.model, m.ready ? (m.simOk ? "jest" : "BRAK") : "-", m.registered ? "jest" : "szukam", m.bars,
               (unsigned)(elapsed() / 1000));
      break;
    }
    case Step::Battery:
      snprintf(b, sizeof b, "%u mV, %u%%%s", battery::millivolts(), battery::percent(), battery::charging() ? ", ładuje" : "");
      break;
    case Step::Network:
      snprintf(b, sizeof b, "%s\n%s", net::wifiConnected() ? ("Wi-Fi: " + net::wifiSsid()).c_str() : "Wi-Fi: brak",
               net::cloudName(net::cloud()));
      break;
    default: return;
  }
  lv_label_set_text(liveLbl, b);
}

std::string missingButtons() {
  std::string s;
  for (int i = 0; i < NBTN; i++)
    if (!pressed[i]) s += (s.empty() ? "" : ", ") + std::string(kButtons[i].name) + " GPIO" + std::to_string(kButtons[i].gpio);
  return s;
}

}  // namespace

void start() {
  on = true;
  step = Step::Screen;
  for (int i = 0; i < STEPS; i++) {
    results[i] = Res::None;
    notes[i].clear();
  }
  for (bool& p : pressed) p = false;
  wheelRight = wheelLeft = 0;
  wheelPhaseLeft = false;
  stepAt = millis();
  Serial.println("[test] start testu sprzetu");
  draw();
}

void stop() {
  on = false;
  ui::setContentWide(false);
}

bool active() { return on; }

void onKey(Key k, Ev type) {
  bool yes = k == Key::Top && type == Ev::KeyShort;
  bool no = k == Key::Bottom && type == Ev::KeyShort;
  switch (step) {
    case Step::Screen:
      if (yes) finish(Res::Pass);
      if (no)
        finish(Res::Fail, "złe kolory: negatyw - przełącz LUMI_PANEL_INVERT; czerwony z niebieskim zamienione - rgb_order w Display.cpp");
      break;
    case Step::Buttons:
      for (int i = 0; i < NBTN; i++)
        if (kButtons[i].key == k && !pressed[i]) {
          pressed[i] = true;
          audio::sound(audio::Sound::Key);
          draw();
        }
      {
        bool all = true;
        for (bool p : pressed) all &= p;
        if (all) finish(Res::Pass);
      }
      break;
    case Step::Wheel:
    case Step::Modem:
    case Step::Battery:
    case Step::Network:
      if (yes) {
        if (step == Step::Battery) {
          uint16_t mv = battery::millivolts();
          finish(mv > 3000 && mv < 4400 ? Res::Pass : Res::Fail,
                 mv > 3000 && mv < 4400 ? "" : "napięcie poza zakresem: dzielnik na GPIO1 albo AT+CBC z SIM800L");
        } else if (step == Step::Network) {
          net::Cloud c = net::cloud();
          finish(c == net::Cloud::Ok || c == net::Cloud::Unclaimed ? Res::Pass : Res::Fail, net::cloudName(c));
        } else {
          finish(Res::Fail, "pominięty");
        }
      }
      break;
    case Step::Sound:
      if (yes) finish(Res::Pass);
      if (no) finish(Res::Fail, "głośnik: SPK+/SPK- modemu (Allegro) albo PAM8302 i GPIO14");
      break;
    case Step::Summary:
      if (type == Ev::KeyShort) stop();
      break;
  }
}

void onWheel(int steps) {
  if (step != Step::Wheel) return;
  if (steps > 0) wheelRight += steps;
  else wheelLeft -= steps;
  if (!wheelPhaseLeft && wheelRight >= 3) {
    wheelPhaseLeft = true;
    draw();
  } else if (!wheelPhaseLeft && wheelLeft >= 3) {
    finish(Res::Fail, "kierunek odwrócony: zamień CLK i DT (GPIO40/41)");
    return;
  } else if (wheelPhaseLeft && wheelLeft >= 3) {
    finish(Res::Pass);
    return;
  }
  updateLive();
}

void tick() {
  if (!on) return;
  uint32_t now = millis();
  if (now - lastDraw > 500) {
    lastDraw = now;
    updateLive();
  }
  switch (step) {
    case Step::Buttons:
      if (elapsed() > 60000) finish(Res::Fail, "brak reakcji: " + missingButtons() + " (druga nóżka do GND)");
      break;
    case Step::Wheel:
      if (elapsed() > 30000) finish(Res::Fail, "brak impulsów: CLK GPIO40, DT GPIO41, zasilanie modułu 3V3");
      break;
    case Step::Modem: {
      modem::Status m = modem::status();
      if (m.ready && m.simOk && m.registered) {
        finish(Res::Pass, std::string(modem::vendorName(m.vendor)) + (m.op[0] ? ", " : "") + m.op);
      } else if (elapsed() > 120000) {
        if (!m.ready) finish(Res::Fail, "nie odpowiada: TX GPIO47 przez dzielnik do RXD, RXD modemu do GPIO48 (v1.0: 38), wspólna masa, 3,7-4,2 V");
        else if (!m.simOk) finish(Res::Fail, "brak karty SIM albo włączony PIN");
        else finish(Res::Fail, "brak sieci: antena, zasięg 2G (SIM800L) lub VoLTE u operatora");
      }
      break;
    }
    case Step::Sound:
      if (now - lastBeep > 3000) {
        lastBeep = now;
        audio::sound(audio::Sound::Confirm);
      }
      break;
    default: break;
  }
}

}  // namespace selftest
