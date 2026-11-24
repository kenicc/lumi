#pragma once
// Polskie nazwy dni, miesięcy i pory dnia. Pora dnia pomaga w orientacji
// osobom z zaburzeniami pamięci ("Wtorek wieczorem" mówi więcej niż "18:40").
#include <stddef.h>

namespace cal {

const char* weekday(int wday);        // 0 = niedziela, z wielkiej litery
const char* monthGenitive(int month); // 1..12: "stycznia"
const char* partOfDay(int hour);      // "rano", "przed południem", "po południu", "wieczorem", "w nocy"

// "Wtorek wieczorem"
void dayLine(int wday, int hour, bool withPart, char* out, size_t n);
// "24 listopada"
void dateLine(int mday, int month, char* out, size_t n);

}  // namespace cal
