#pragma once
// Pogoda: kody WMO z Open-Meteo -> słowo po polsku, ikona i krótka rada.
#include <stdint.h>

namespace wx {

enum class Icon : uint8_t { Sun, CloudSun, Cloud, Fog, Rain, Shower, Snow, Storm };

struct Day {
  bool valid = false;
  int code = 0;
  float tMin = 0, tMax = 0;
};

struct Forecast {
  bool valid = false;
  float tempNow = 0;
  int codeNow = 0;
  Day today, tomorrow;
  uint32_t fetchedAt = 0;  // millis()
  int dayKey = -1;         // dzień pobrania: rok * 1000 + dzień roku (czas lokalny); -1 = nieznany
};

constexpr uint32_t STALE_AFTER_MS = 3UL * 60 * 60 * 1000;
// Prognoza taka, jaką wolno pokazać teraz: temperatura sprzed ponad 3 godzin nie jest "teraz"
// (valid = false), a po północy wczorajsze "jutro" staje się "dziś". Starsza niż dzień - nic.
Forecast fresh(Forecast f, uint32_t nowMs, int todayKey);

Icon iconFor(int wmo);
const char* textFor(int wmo);  // "pochmurno"
// Jedna rada zamiast tabeli liczb: "Weź parasol", "Uwaga, ślisko" albo "".
const char* hintFor(int wmo, float tMin);

}  // namespace wx
