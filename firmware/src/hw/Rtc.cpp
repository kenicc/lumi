#include "Rtc.h"

#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>

#include "../app/Events.h"
#include "../core/RtcCodec.h"
#include "Board.h"

namespace rtc {

namespace {

constexpr uint8_t ADDR = 0x68;
constexpr uint8_t REG_STATUS = 0x0F;
constexpr uint8_t OSF = 0x80;  // oscylator stał (bateria padła albo moduł nowy) - czas niepewny
bool found = false;
SemaphoreHandle_t mtx = nullptr;

bool readRegs(uint8_t start, uint8_t* out, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(start);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) out[i] = Wire.read();
  return true;
}

bool writeRegs(uint8_t start, const uint8_t* in, uint8_t n) {
  Wire.beginTransmission(ADDR);
  Wire.write(start);
  Wire.write(in, n);
  return Wire.endTransmission() == 0;
}

}  // namespace

void begin() {
  mtx = xSemaphoreCreateMutex();
  Wire.begin(pins::I2C_SDA, pins::I2C_SCL, 100000);
  Wire.beginTransmission(ADDR);
  found = Wire.endTransmission() == 0;
  if (!found) {
    Serial.println("[zegar] brak modulu DS3231 - godzina z Wi-Fi albo z sieci komorkowej");
    return;
  }
  uint8_t status = 0, regs[7];
  if (!readRegs(REG_STATUS, &status, 1) || !readRegs(0x00, regs, 7)) {
    Serial.println("[zegar] DS3231 nie odpowiada poprawnie");
    return;
  }
  int64_t utc;
  if ((status & OSF) || !rtcc::decode(regs, utc)) {
    Serial.println("[zegar] DS3231 bez czasu (nowy albo wymien baterie CR2032) - ustawi sie z sieci");
    return;
  }
  timeval tv{(time_t)utc, 0};
  settimeofday(&tv, nullptr);
  Serial.println("[zegar] godzina z modulu DS3231");
  bus::post(Event(Ev::NetTime, 0, 0, "rtc"));
}

bool present() { return found; }

void saveNow() {
  if (!found || !mtx) return;
  time_t now = time(nullptr);
  if (now < 1700000000) return;  // nie zapisuj czasu, którego sami nie znamy
  uint8_t regs[7];
  rtcc::encode((int64_t)now, regs);
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool ok = writeRegs(0x00, regs, 7);
  uint8_t status = 0;
  if (ok && readRegs(REG_STATUS, &status, 1) && (status & OSF)) {
    status &= ~OSF;
    writeRegs(REG_STATUS, &status, 1);
  }
  xSemaphoreGive(mtx);
  if (!ok) Serial.println("[zegar] zapis do DS3231 nieudany");
}

}  // namespace rtc
