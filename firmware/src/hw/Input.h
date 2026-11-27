#pragma once
// Przyciski i rolka głośności. Wysyła zdarzenia Key*/Wheel do kolejki.
#include "../app/Events.h"
#include "../core/ButtonFilter.h"

namespace input {

void begin();
void poll();  // co ~5 ms z pętli głównej

// Ustawienia dostępności z konfiguracji (drżenie dłoni, tryb jednego przycisku)
void configure(uint16_t minPressMs, uint16_t repeatIgnoreMs, bool oneButton);

// Symulacja przycisku z konsoli (testy interfejsu bez lutowania)
void inject(Key k, BtnEvent e);

// Podgląd w konsoli: każde naciśnięcie, puszczenie i obrót rolki (sprawdzanie okablowania bez ekranu)
void setTrace(bool on);
bool trace();

const char* keyName(Key k);
bool keyFromName(const char* s, Key& out);

}  // namespace input
