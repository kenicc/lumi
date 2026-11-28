#pragma once
// Kolejka przypomnień: co pokazać, kiedy zadzwonić ponownie, kiedy uznać za niepotwierdzone.
// Czysta logika (czas podawany z zewnątrz) - testowana w test/test_core.
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "Config.h"

namespace rem {

struct Item {
  std::string id, text;
  int minute = 0;
  bool confirm = true;
  uint8_t snoozes = 0;
  uint32_t fireAt = 0;  // ms; 0 = od razu
};

enum class Timeout : uint8_t { None, Missed, Shown };

// Które minuty (od 1970, czas lokalny nieistotny) sprawdzić teraz, skoro ostatnio sprawdzona była
// `last` (-1 = jeszcze żadna). Spóźniona pętla nie gubi przypomnień - nadrabiamy do CATCH_UP minut.
// Cofnięcie zegara nie powtarza przypomnień, a duży skok (np. pierwsze ustawienie czasu) zaczyna
// od bieżącej minuty, zamiast wysypać wszystko z przeszłości. Zwraca false, gdy nic do sprawdzenia.
constexpr long CATCH_UP_MIN = 10;
bool dueWindow(long last, long now, long& from, long& to);

class Queue {
 public:
  static constexpr uint32_t SNOOZE_MS = 10UL * 60 * 1000;
  static constexpr uint32_t TIMEOUT_MS = 10UL * 60 * 1000;
  static constexpr uint32_t BEEP_MS = 60UL * 1000;
  static constexpr uint8_t MAX_SNOOZES = 2;

  // Dopisuje przypomnienia wypadające w tej minucie (wday jak w tm: 0 = niedziela)
  void addDue(const std::vector<cfg::Reminder>& list, int wday, int minuteOfDay);
  void add(const Item& it) { q_.push_back(it); }

  // Uruchamia pierwsze gotowe przypomnienie; true = trzeba je pokazać
  bool start(uint32_t now);
  bool active() const { return active_; }
  const Item& current() const { return cur_; }
  bool canSnooze() const { return active_ && cur_.snoozes < MAX_SNOOZES; }
  bool snooze(uint32_t now);  // odłóż o SNOOZE_MS; false = już nie można
  void finish() { active_ = false; }
  bool pending() const { return !q_.empty(); }

  struct Tick {
    bool beep = false;
    Timeout timeout = Timeout::None;
  };
  // Wywoływać często. W trakcie rozmowy czas się nie liczy (rozmowa nie zjada czasu na potwierdzenie).
  Tick tick(uint32_t now, bool inCall, uint32_t dt);

 private:
  std::deque<Item> q_;
  bool active_ = false;
  Item cur_;
  uint32_t shownAt_ = 0, beepAt_ = 0;
};

}  // namespace rem
