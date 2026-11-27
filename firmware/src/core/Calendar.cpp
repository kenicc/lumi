#include "Calendar.h"

#include <stdio.h>

namespace cal {

const char* weekday(int wday) {
  static const char* const k[] = {"Niedziela", "Poniedziałek", "Wtorek", "Środa", "Czwartek", "Piątek", "Sobota"};
  return (wday >= 0 && wday < 7) ? k[wday] : "";
}

const char* monthGenitive(int month) {
  static const char* const k[] = {"stycznia", "lutego",   "marca",       "kwietnia", "maja",      "czerwca",
                                  "lipca",    "sierpnia", "września", "października", "listopada", "grudnia"};
  return (month >= 1 && month <= 12) ? k[month - 1] : "";
}

const char* partOfDay(int hour) {
  if (hour >= 5 && hour < 10) return "rano";
  if (hour >= 10 && hour < 12) return "przed południem";
  if (hour >= 12 && hour < 18) return "po południu";
  if (hour >= 18 && hour < 22) return "wieczorem";
  return "w nocy";
}

void dayLine(int wday, int hour, bool withPart, char* out, size_t n) {
  if (withPart)
    snprintf(out, n, "%s %s", weekday(wday), partOfDay(hour));
  else
    snprintf(out, n, "%s", weekday(wday));
}

void dateLine(int mday, int month, char* out, size_t n) { snprintf(out, n, "%d %s", mday, monthGenitive(month)); }

}  // namespace cal
