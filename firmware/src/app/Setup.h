#pragma once
// Ustawienia na samym LUMI, bez telefonu: Wi-Fi (lista sieci, hasło wpisywane rolką), kontakty
// (z karty SIM albo wpisane), wielkość tekstu, dzwonek, dodanie do aplikacji rodziny, informacje.
// Sterowanie wszędzie takie samo: rolka albo górny/dolny = przesuwanie, środkowy = wybór,
// zielony Dzwoń = gotowe, Dom = wstecz (przytrzymany: do zegara).
#include "Events.h"

namespace localsetup {

void open();     // wejście do menu (strona główna)
void render();   // rysuje bieżącą stronę w ui::content()
bool onKey(Key k, Ev type);  // true = wyjście z menu
void onWheel(int d);
void onEvent(Ev e);  // Ev::WifiScan, Ev::SimBook

}  // namespace localsetup
