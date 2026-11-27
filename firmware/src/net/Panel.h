#pragma once
// Panel w przeglądarce do testów prototypu: wirtualne przyciski i rolka LUMI.
// Działa, gdy LUMI jest w domowej sieci Wi-Fi: http://<adres LUMI>:8080 (adres w konsoli: panel).
// Tylko do prototypu - bez logowania, każdy w tej samej sieci może naciskać przyciski.
#include <string>

namespace panel {

void begin();             // nic nie robi, jeśli wariant nie ma LUMI_WEB_PANEL
std::string address();    // "http://192.168.1.23:8080" albo "" gdy brak sieci lub panel wyłączony

}  // namespace panel
