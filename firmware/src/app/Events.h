#pragma once
// Zdarzenia między zadaniami. Modem, audio i sieć działają w osobnych zadaniach
// i tylko wysyłają zdarzenia - interfejs (LVGL) dotyka wyłącznie zadanie główne.
#include <stdint.h>
#include <string.h>

enum class Key : uint8_t { Top, Mid, Bottom, Music, Home, Call, Power, Count };

enum class Ev : uint8_t {
  KeyShort,
  KeyHoldStart,
  KeyLong,
  Wheel,  // a = kroki (+ głośniej)

  ModemReady,  // text = nazwa modemu
  ModemFail,   // modem nie odpowiada
  NetTime,     // czas ustawiony z sieci komórkowej lub NTP

  CallIncoming,  // text = numer ("" = zastrzeżony)
  CallDialing,
  CallActive,
  CallEnded,  // a = 1 gdy rozmowa była odebrana

  WeatherUpdated,
  ConfigUpdated,  // nowy plik konfiguracji zapisany przez synchronizację
  MediaState,     // zmiana stanu odtwarzania
  CloudState,     // a = net::Cloud (Wi-Fi, parowanie, serwer)
  PhotosUpdated,  // pobrano nowe zdjęcia kontaktów
};

struct Event {
  Ev type;
  int32_t a = 0;
  int32_t b = 0;
  char text[40] = {0};

  Event() : type(Ev::KeyShort) {}
  explicit Event(Ev t, int32_t a_ = 0, int32_t b_ = 0, const char* s = nullptr) : type(t), a(a_), b(b_) {
    if (s) {
      strncpy(text, s, sizeof(text) - 1);
      text[sizeof(text) - 1] = 0;
    }
  }
};

namespace bus {
void begin();
bool post(const Event& e);  // bezpieczne z każdego zadania
bool receive(Event& e, uint32_t timeoutMs);
}  // namespace bus
