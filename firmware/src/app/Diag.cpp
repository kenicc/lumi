#include "app/Diag.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <stdarg.h>

namespace diag {

namespace {
constexpr size_t MAX_LINES = 8;
SemaphoreHandle_t mtx;
std::vector<std::string> lines;
unsigned boots = 0;
const char* reason = "?";

const char* reasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "zasilanie";
    case ESP_RST_SW: return "programowy";
    case ESP_RST_PANIC: return "blad programu";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "straznik";
    case ESP_RST_DEEPSLEEP: return "wybudzenie";
    case ESP_RST_BROWNOUT: return "spadek napiecia";
    case ESP_RST_EXT: return "przycisk RESET";
    default: return "inny";
  }
}
}  // namespace

void begin() {
  mtx = xSemaphoreCreateMutex();
  reason = reasonName(esp_reset_reason());
  Preferences p;
  p.begin("lumi-diag", false);
  boots = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", boots);
  p.end();
  Serial.printf("[diag] uruchomienie %u, powod restartu: %s\n", boots, reason);
}

void error(const char* fmt, ...) {
  char b[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  Serial.println(b);
  if (!mtx) return;
  char t[16];
  snprintf(t, sizeof t, "%lus ", (unsigned long)(millis() / 1000));
  xSemaphoreTake(mtx, portMAX_DELAY);
  lines.push_back(std::string(t) + b);
  if (lines.size() > MAX_LINES) lines.erase(lines.begin());
  xSemaphoreGive(mtx);
}

const char* resetReason() { return reason; }
unsigned bootCount() { return boots; }

std::vector<std::string> recent() {
  if (!mtx) return {};
  xSemaphoreTake(mtx, portMAX_DELAY);
  auto copy = lines;
  xSemaphoreGive(mtx);
  return copy;
}

}  // namespace diag
