#include "Weather.h"

namespace wx {

Icon iconFor(int c) {
  if (c == 0) return Icon::Sun;
  if (c == 1 || c == 2) return Icon::CloudSun;
  if (c == 3) return Icon::Cloud;
  if (c == 45 || c == 48) return Icon::Fog;
  if ((c >= 51 && c <= 67)) return Icon::Rain;
  if (c >= 71 && c <= 77) return Icon::Snow;
  if (c >= 80 && c <= 82) return Icon::Shower;
  if (c == 85 || c == 86) return Icon::Snow;
  if (c >= 95) return Icon::Storm;
  return Icon::Cloud;
}

const char* textFor(int c) {
  if (c == 0) return "słonecznie";
  if (c == 1) return "prawie bezchmurnie";
  if (c == 2) return "częściowo pochmurno";
  if (c == 3) return "pochmurno";
  if (c == 45 || c == 48) return "mgła";
  if (c >= 51 && c <= 57) return "mżawka";
  if (c >= 61 && c <= 67) return "deszcz";
  if (c >= 71 && c <= 77) return "śnieg";
  if (c >= 80 && c <= 82) return "przelotny deszcz";
  if (c == 85 || c == 86) return "przelotny śnieg";
  if (c >= 95) return "burza";
  return "";
}

const char* hintFor(int c, float tMin) {
  bool snow = (c >= 71 && c <= 77) || c == 85 || c == 86;
  bool freezing = c == 56 || c == 57 || c == 66 || c == 67;
  bool wet = (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95;
  if (snow || freezing || (wet && tMin <= 1.0f)) return "Uwaga, ślisko";
  if (wet) return "Weź parasol";
  if (tMin <= -10.0f) return "Bardzo zimno";
  return "";
}

Forecast fresh(Forecast f, uint32_t nowMs, int todayKey) {
  if (!f.valid) return f;
  if (nowMs - f.fetchedAt > STALE_AFTER_MS) f.valid = false;
  if (f.dayKey >= 0 && todayKey >= 0 && todayKey != f.dayKey) {
    // następny dzień (także przez Nowy Rok: rok+1, dzień 0) - jutro staje się dziś
    bool nextDay = todayKey == f.dayKey + 1 || (todayKey / 1000 == f.dayKey / 1000 + 1 && todayKey % 1000 == 0);
    f.today = nextDay ? f.tomorrow : Day{};
    f.tomorrow = Day{};
    if (!nextDay) f.valid = false;
  }
  return f;
}

}  // namespace wx
