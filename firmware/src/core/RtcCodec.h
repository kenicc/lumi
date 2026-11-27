#pragma once
// Rejestry czasu zegara DS3231 (0x00-0x06, BCD) <-> czas UTC w sekundach.
// Zegar trzyma UTC: strefę i czas letni liczy system (TZ), więc zmiana czasu nie wymaga zapisu.
#include <stdint.h>

namespace rtcc {

void encode(int64_t utc, uint8_t regs[7]);
// false, gdy rejestry nie wyglądają na prawidłowy czas (np. nowy moduł albo wyczerpana bateria)
bool decode(const uint8_t regs[7], int64_t& utc);

}  // namespace rtcc
