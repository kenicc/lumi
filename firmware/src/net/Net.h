#pragma once
// Sieć: Wi-Fi (wyłączane w trybie FM), czas z NTP, pogoda (Open-Meteo, bez klucza)
// i połączenie z serwerem aplikacji rodziny:
//   POST /api/device/hello   - rejestracja (id, sekret, kod parowania)
//   GET  /api/device/config  - dokument konfiguracji, gdy jest nowsza wersja
//   POST /api/device/status  - bateria, zasięg, wersja (rodzina widzi stan w aplikacji)
// Gdy urządzenie nie zna żadnej sieci, wystawia własną (LUMI-XXXXXX) ze stroną konfiguracji Wi-Fi.
#include <string>
#include <vector>

#include "../core/Config.h"
#include "../core/Weather.h"

namespace net {

enum class Cloud : uint8_t {
  NoWifi,        // nie zna żadnej sieci
  Provisioning,  // wystawia sieć LUMI-XXXXXX do konfiguracji z telefonu
  Connecting,    // łączy z Wi-Fi
  NoServer,      // brak adresu serwera
  Offline,       // Wi-Fi jest, serwer nie odpowiada
  Unclaimed,     // serwer zna urządzenie, ale nikt go nie dodał w aplikacji
  Ok,
  AuthError,     // serwer odrzuca sekret (urządzenie przypisane gdzie indziej)
};

struct Settings {
  std::vector<cfg::Wifi> wifi;
  float lat = 0, lon = 0;
  bool weather = true;
  uint16_t syncIntervalSec = 20;
  int version = 0;
  std::vector<std::string> photos;  // skróty zdjęć kontaktów do pobrania
};

void begin();
void apply(const Settings& s);
bool wifiConnected();
std::string wifiSsid();
Cloud cloud();
const char* cloudName(Cloud c);
wx::Forecast forecast();
void refreshWeather();
void reportSoon();  // wyślij stan przy najbliższej okazji (np. po zastosowaniu konfiguracji)
void startProvisioning();
void checkUpdateNow();

// Zdarzenie dla rodziny (np. potwierdzenie przypomnienia) - czeka w kolejce, gdy brak internetu
void queueEvent(const std::string& json);

// Wiadomości od rodziny pobrane z serwera (każda tylko raz)
struct Message {
  int id = 0;
  std::string author, text;
  long long at = 0;
};
std::vector<Message> takeMessages();  // konsola: sprawdź i zainstaluj aktualizację od razu (bez czekania na noc)

bool saveConfigFile(const char* json, size_t len);

// Ścieżka zdjęcia kontaktu w LittleFS (96x96 RGB565, 18 432 B)
std::string photoPath(const std::string& hash);
constexpr size_t PHOTO_BYTES = 96 * 96 * 2;

}  // namespace net
