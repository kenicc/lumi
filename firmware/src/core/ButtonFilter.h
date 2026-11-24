#pragma once
// Filtr jednego przycisku: drgania styków, drżenie dłoni, przytrzymanie.
// Czysta logika bez Arduino - testowana w test/test_core.
#include <stdint.h>

enum class BtnEvent : uint8_t {
  None,
  Short,      // zwykłe naciśnięcie
  HoldStart,  // przycisk z przytrzymaniem: zaczęło się trzymanie (UI pokazuje postęp)
  Long,       // przytrzymanie osiągnęło longMs
};

struct ButtonTiming {
  uint16_t debounceMs = 30;      // drgania styków
  uint16_t minPressMs = 0;       // drżenie dłoni: krótsze dotknięcia są ignorowane
  uint16_t repeatIgnoreMs = 250; // drżenie dłoni: powtórzenia tuż po naciśnięciu są ignorowane
  uint16_t longMs = 0;           // 0 = przycisk bez przytrzymania (Short od razu po przyjęciu)
};

class ButtonFilter {
 public:
  void setTiming(const ButtonTiming& t) { t_ = t; }
  const ButtonTiming& timing() const { return t_; }

  // raw = true gdy przycisk wciśnięty; wywoływać co kilka ms
  BtnEvent update(bool raw, uint32_t now) {
    if (raw != lastRaw_) {
      lastRaw_ = raw;
      rawChangedAt_ = now;
    }
    if (stable_ != lastRaw_ && now - rawChangedAt_ >= t_.debounceMs) {
      stable_ = lastRaw_;
      if (stable_) return onPress(now);
      return onRelease(now);
    }
    if (stable_ && !suppressed_) return onHeld(now);
    return BtnEvent::None;
  }

  bool isHeld() const { return stable_ && accepted_ && !suppressed_; }
  uint32_t heldMs(uint32_t now) const { return stable_ ? now - pressedAt_ : 0; }

 private:
  BtnEvent onPress(uint32_t now) {
    pressedAt_ = now;
    accepted_ = false;
    longFired_ = false;
    suppressed_ = everAccepted_ && (now - lastAcceptAt_ < t_.repeatIgnoreMs);
    return suppressed_ ? BtnEvent::None : onHeld(now);
  }

  BtnEvent onHeld(uint32_t now) {
    uint32_t held = now - pressedAt_;
    if (!accepted_ && held >= t_.minPressMs) {
      accepted_ = true;
      if (t_.longMs == 0) {
        accept(now);
        return BtnEvent::Short;
      }
      return BtnEvent::HoldStart;
    }
    if (t_.longMs && accepted_ && !longFired_ && held >= t_.longMs) {
      longFired_ = true;
      accept(now);
      return BtnEvent::Long;
    }
    return BtnEvent::None;
  }

  BtnEvent onRelease(uint32_t now) {
    bool wasShort = t_.longMs && accepted_ && !longFired_ && !suppressed_;
    accepted_ = false;
    suppressed_ = false;
    if (wasShort) {
      accept(now);
      return BtnEvent::Short;
    }
    return BtnEvent::None;
  }

  void accept(uint32_t now) {
    lastAcceptAt_ = now;
    everAccepted_ = true;
  }

  ButtonTiming t_;
  bool lastRaw_ = false, stable_ = false;
  bool accepted_ = false, longFired_ = false, suppressed_ = false, everAccepted_ = false;
  uint32_t rawChangedAt_ = 0, pressedAt_ = 0, lastAcceptAt_ = 0;
};
