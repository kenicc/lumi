#pragma once
// Wi-Fi, czas z NTP, pogoda (Open-Meteo, bez klucza) i pobieranie konfiguracji od rodziny.
// W trybie FM Wi-Fi jest wyłączane (cisza elektryczna wokół anteny).
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Weather.h"

namespace net {

struct Settings {
  std::vector<cfg::Wifi> wifi;
  float lat = 0, lon = 0;
  bool weather = true;
  std::string syncUrl;
  uint16_t syncIntervalSec = 300;
  int version = 0;
};

void begin(const char* deviceId);
void apply(const Settings& s);  // po każdej zmianie konfiguracji
bool wifiConnected();
wx::Forecast forecast();
void refreshWeather();

// Zapis konfiguracji: najpierw plik tymczasowy, potem zamiana - przerwa w zasilaniu nie psuje pliku.
bool saveConfigFile(const char* json, size_t len);

}  // namespace net
