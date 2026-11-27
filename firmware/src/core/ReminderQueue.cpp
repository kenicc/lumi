#include "ReminderQueue.h"

namespace rem {

void Queue::addDue(const std::vector<cfg::Reminder>& list, int wday, int minuteOfDay) {
  for (const auto& r : list)
    if (cfg::reminderDue(r, wday, minuteOfDay)) q_.push_back({r.id, r.text, r.minute, r.confirm, 0, 0});
}

bool Queue::start(uint32_t now) {
  if (active_) return false;
  for (auto it = q_.begin(); it != q_.end(); ++it) {
    if (it->fireAt && (int32_t)(now - it->fireAt) < 0) continue;  // odłożone - jeszcze nie pora
    cur_ = *it;
    q_.erase(it);
    active_ = true;
    shownAt_ = beepAt_ = now;
    return true;
  }
  return false;
}

bool Queue::snooze(uint32_t now) {
  if (!canSnooze()) return false;
  cur_.snoozes++;
  cur_.fireAt = now + SNOOZE_MS;
  q_.push_back(cur_);
  active_ = false;
  return true;
}

Queue::Tick Queue::tick(uint32_t now, bool inCall, uint32_t dt) {
  Tick t;
  if (!active_) return t;
  if (inCall) {
    shownAt_ += dt;
    beepAt_ += dt;
    return t;
  }
  if (now - beepAt_ >= BEEP_MS) {
    beepAt_ = now;
    t.beep = true;
  }
  if (now - shownAt_ > TIMEOUT_MS) {
    t.timeout = cur_.confirm ? Timeout::Missed : Timeout::Shown;
    active_ = false;
  }
  return t;
}

}  // namespace rem
