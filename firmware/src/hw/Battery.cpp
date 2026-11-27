#include "Battery.h"

#include <Arduino.h>

#include "Board.h"
#include "../modem/Modem.h"

namespace battery {

namespace {

uint32_t filtered = 0;  // mV * 16
uint16_t trendRef = 0;
uint32_t trendAt = 0;
bool chg = false;
uint32_t lastPoll = 0;
bool noDivider = false;
bool modemSeen = false;  // był choć jeden odczyt napięcia z modemu  // dzielnik niepodłączony: napięcie bierzemy z modemu (SIM800 AT+CBC)

// Krzywa Li-ion pod lekkim obciążeniem: (mV, %)
const uint16_t kCurve[][2] = {{4150, 100}, {4050, 90}, {3950, 78}, {3870, 65}, {3800, 52}, {3750, 40},
                              {3710, 30},  {3670, 20}, {3620, 12}, {3550, 6},  {3450, 2},  {3300, 0}};

uint16_t readMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(pins::BATTERY_ADC);
  return (uint16_t)((sum / 8) * board::BATTERY_DIVIDER);
}

}  // namespace

void begin() {
  analogSetPinAttenuation(pins::BATTERY_ADC, ADC_11db);
  uint16_t mv = readMv();
  noDivider = mv < 1000;
  if (noDivider) Serial.println("[bateria] brak dzielnika na GPIO1 - napiecie z modemu, jesli je podaje");
  filtered = mv * 16;
  trendRef = mv;
  trendAt = millis();
}

void poll() {
  uint32_t now = millis();
  if (now - lastPoll < 2000) return;
  lastPoll = now;
  uint16_t mv = readMv();
  if (noDivider) {
    int m = modem::status().batteryMv;
    if (m <= 0) return;
    modemSeen = true;
    if (filtered < 16000) filtered = m * 16;  // pierwszy pomiar
    mv = m;
  }
  filtered = filtered - filtered / 16 + mv;  // wygładzanie wykładnicze
  if (now - trendAt > 120000) {
    uint16_t cur = millivolts();
    if (cur > trendRef + 15) chg = true;        // napięcie rośnie -> ładowarka podłączona
    else if (cur + 10 < trendRef) chg = false;  // spada -> praca z baterii
    trendRef = cur;
    trendAt = now;
  }
}

uint16_t millivolts() { return filtered / 16; }

uint8_t percent() {
  uint16_t mv = millivolts();
  if (mv >= kCurve[0][0]) return 100;
  for (size_t i = 1; i < sizeof(kCurve) / sizeof(kCurve[0]); i++) {
    if (mv >= kCurve[i][0]) {
      uint16_t v0 = kCurve[i][0], v1 = kCurve[i - 1][0], p0 = kCurve[i][1], p1 = kCurve[i - 1][1];
      return p0 + (uint32_t)(mv - v0) * (p1 - p0) / (v1 - v0);
    }
  }
  return 0;
}

bool charging() { return chg; }

bool known() { return !noDivider || modemSeen; }

}  // namespace battery
