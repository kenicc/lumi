#pragma once
// Test sprzętu krok po kroku - do pierwszego uruchomienia złożonego prototypu.
// Prowadzi przez ekran, przyciski, rolkę, modem, dźwięk, baterię i sieć; przy błędzie
// podpowiada, który przewód lub pin sprawdzić (na ekranie i w konsoli USB).
// Wejście: przytrzymany Home przy włączaniu albo komenda "selftest" w konsoli.
#include "Events.h"

namespace selftest {

void start();
void stop();
bool active();
void onKey(Key k, Ev type);
void onWheel(int steps);
void tick();  // co ~100 ms z pętli logiki

}  // namespace selftest
