#include "Input.h"

#include <Arduino.h>
#include <string.h>

#include "Board.h"

namespace input {

namespace {

constexpr int kPins[(int)Key::Count] = {pins::BTN_TOP,  pins::BTN_MID,  pins::BTN_BOTTOM, pins::BTN_MUSIC,
                                        pins::BTN_HOME, pins::BTN_CALL, pins::BTN_POWER};
const char* const kNames[(int)Key::Count] = {"top", "mid", "bottom", "music", "home", "call", "power"};

ButtonFilter filters[(int)Key::Count];

constexpr uint16_t POWER_LONG_MS = 3000;  // przytrzymanie 3 s = wyłącz
constexpr uint16_t ONE_BUTTON_LONG_MS = 900;
constexpr uint16_t HOME_SERVICE_MS = 5000;  // przytrzymanie Home 5 s = ekran serwisowy dla rodziny

volatile int32_t wheelSteps = 0;
volatile uint8_t wheelState = 0;
int32_t wheelReported = 0;

// Tablica przejść enkodera kwadraturowego: odporna na drgania styków
const int8_t kQuad[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR onWheel() {
  uint8_t s = (digitalRead(pins::WHEEL_A) << 1) | digitalRead(pins::WHEEL_B);
  wheelState = ((wheelState << 2) | s) & 0x0F;
  wheelSteps += kQuad[wheelState];
}

void emit(Key k, BtnEvent e) {
  Ev t;
  switch (e) {
    case BtnEvent::Short: t = Ev::KeyShort; break;
    case BtnEvent::HoldStart: t = Ev::KeyHoldStart; break;
    case BtnEvent::Long: t = Ev::KeyLong; break;
    default: return;
  }
  bus::post(Event(t, (int32_t)k));
}

}  // namespace

void begin() {
  for (int i = 0; i < (int)Key::Count; i++) pinMode(kPins[i], INPUT_PULLUP);
  pinMode(pins::WHEEL_A, INPUT_PULLUP);
  pinMode(pins::WHEEL_B, INPUT_PULLUP);
  attachInterrupt(pins::WHEEL_A, onWheel, CHANGE);
  attachInterrupt(pins::WHEEL_B, onWheel, CHANGE);
  configure(0, 250, false);
}

void configure(uint16_t minPressMs, uint16_t repeatIgnoreMs, bool oneButton) {
  for (int i = 0; i < (int)Key::Count; i++) {
    ButtonTiming t;
    t.minPressMs = minPressMs;
    t.repeatIgnoreMs = repeatIgnoreMs;
    if (i == (int)Key::Power) t.longMs = POWER_LONG_MS;
    if (i == (int)Key::Mid && oneButton) t.longMs = ONE_BUTTON_LONG_MS;
    if (i == (int)Key::Home) t.longMs = HOME_SERVICE_MS;
    filters[i].setTiming(t);
  }
}

void poll() {
  uint32_t now = millis();
  for (int i = 0; i < (int)Key::Count; i++) {
    BtnEvent e = filters[i].update(digitalRead(kPins[i]) == LOW, now);
    if (e != BtnEvent::None) emit((Key)i, e);
  }
  int32_t steps = wheelSteps / board::WHEEL_STEPS_PER_DETENT;
  if (steps != wheelReported) {
    bus::post(Event(Ev::Wheel, steps - wheelReported));
    wheelReported = steps;
  }
}

void inject(Key k, BtnEvent e) { emit(k, e); }

const char* keyName(Key k) { return (int)k < (int)Key::Count ? kNames[(int)k] : "?"; }

bool keyFromName(const char* s, Key& out) {
  for (int i = 0; i < (int)Key::Count; i++)
    if (!strcmp(s, kNames[i])) {
      out = (Key)i;
      return true;
    }
  return false;
}

}  // namespace input
