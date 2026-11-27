#pragma once
// Wspólny stan logiki urządzenia (App.cpp) i ekranów (Screens.cpp).
// Tylko dla plików w src/app - reszta oprogramowania używa interfejsu z App.h.
#include <lvgl.h>
#include <stdint.h>
#include <time.h>

#include <atomic>
#include <deque>
#include <string>

#include "../core/Config.h"
#include "../core/ReminderQueue.h"
#include "../net/Net.h"

namespace app {
namespace detail {

enum class Screen : uint8_t { Home, Contacts, Media, Weather, Confirm, Unpaired, Service, Reminder, Message };
enum class Phase : uint8_t { None, Incoming, Outgoing, Active };

struct Call {
  Phase phase = Phase::None;
  char number[24] = {0};
  int contact = -1;
  uint32_t startedAt = 0;
  bool silenced = false;
  bool rejected = false;
  bool hungUpHere = false;  // rozłączył senior (bez komunikatu "Nikt nie odebrał")
  uint32_t autoAt = 0;      // automatyczne odbieranie
  uint8_t autoStage = 0;
  bool autoAnswered = false;
};

struct Missed {
  bool has = false;
  char number[24] = {0};
  time_t at = 0;
  int count = 0;
};

// --- stan (definicje w App.cpp) ---
extern cfg::Config C;
extern bool haveConfig;
extern char devId[24];
extern Screen scr;
extern int contactIdx, mediaIdx, confirmIdx, playingIdx;
extern Call call;
extern Missed missed;
extern rem::Queue reminders;
extern std::deque<net::Message> inbox;  // nieprzeczytane wiadomości, pierwsza jest na ekranie
extern bool modemReady, simOk, modemFailed;
extern int highlight;
extern lv_obj_t* live;  // etykieta odświeżana co sekundę (czas rozmowy, odliczanie)

// --- pomocnicze (App.cpp) ---
bool mediaOn();
const cfg::Contact* contactAt(int i);
bool timeValid();
tm now_tm();

template <typename T>
T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

int contactIndexByName(const std::string& name);  // -1 gdy autor wiadomości nie jest w kontaktach

// --- ekrany (Screens.cpp) ---
void render();
void go(Screen s);
void updateLive();
const lv_image_dsc_t* photoFor(const cfg::Contact* k);
void freeUnusedPhotos();
void dropMissingPhotos();

}  // namespace detail
}  // namespace app
