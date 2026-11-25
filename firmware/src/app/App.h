#pragma once
// Logika urządzenia: ekrany, znaczenie przycisków, pierwszeństwo rozmowy.
// Działa w zadaniu głównym (rdzeń 0) - jedyne miejsce, które dotyka LVGL.
#include <stdint.h>

#include "app/Events.h"
#include "core/Config.h"

namespace app {

void begin();
void loop();  // obsługa zdarzeń i timerów, bez blokowania

const cfg::Config& config();
bool configured();
bool reloadConfig(bool announce);  // z /config.json; announce = pokaż, kto zmienił
const char* deviceId();
void showNotice(const char* text);

// LUMI nic nie robi (ekran zgaszony, bez rozmowy i muzyki) - można zainstalować aktualizację.
// Czytane z zadania sieci, dlatego atomowe.
bool idleForUpdate();
bool inCall();

}  // namespace app
