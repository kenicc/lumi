#pragma once
// Poziom baterii z dzielnika napięcia na ADC. Ładowanie rozpoznawane po rosnącym napięciu
// (w prototypie nie ma sygnału z TP4056) - to przybliżenie, do zmierzenia na stole.
#include <stdint.h>

namespace battery {

void begin();
void poll();  // co kilka sekund
uint16_t millivolts();
uint8_t percent();
bool charging();

}  // namespace battery
