// Tor dźwięku prototypu z Allegro: bez DAC, wzmacniacza i tunera FM.
// Głośnik 8 Ω i mikrofon elektretowy są podłączone do SIM800L (SPK+/-, MIC+/-),
// więc dzwonek gra modem (głośność AT+CRSL), a sygnały interfejsu to tony AT+SIMTONE.
// Ten sam interfejs co AudioOut.cpp - reszta oprogramowania nie wie, który wariant działa.
#include <Arduino.h>

#include "app/Events.h"
#include "audio/AudioOut.h"
#include "modem/Modem.h"

namespace audio {

namespace {
State st;
bool keyClick = true;
bool inCall = false;
uint8_t ringLevel = 8;
}  // namespace

void begin(uint8_t startVolume) { st.volume = startVolume; }

void playFm(float) {
  st.failed = true;
  bus::post(Event(Ev::MediaState));
}
void playUrls(const std::vector<std::string>&) {
  st.failed = true;
  bus::post(Event(Ev::MediaState));
}
void stop() {
  st.playing = false;
  st.failed = false;
}
void setVolume(uint8_t level) { st.volume = level; }
void setMaxVolume(uint8_t) {}
void setKeyClick(bool on) { keyClick = on; }

void sound(Sound s) {
  if (inCall) return;
  switch (s) {
    case Sound::Key:
      if (keyClick) modem::tone(880, 40, 0, 40);
      break;
    case Sound::Confirm: modem::tone(784, 120, 0, 120); break;
    case Sound::Error: modem::tone(440, 300, 0, 300); break;
    case Sound::Notice: modem::tone(660, 150, 100, 500); break;
    case Sound::AutoAnswer: modem::tone(784, 180, 120, 900); break;  // trzy sygnały
  }
}

// Modem dzwoni sam po przyjściu rozmowy; tu ustawiamy tylko głośność (0 = cisza)
void ring(bool on, uint8_t level) {
  if (on) ringLevel = level;
  modem::ringer(on ? level : 0);  // głośność wraca w callMode(false) po zakończeniu rozmowy
}

void callMode(bool on) {
  inCall = on;
  if (!on) modem::ringer(ringLevel);  // po rozmowie dzwonek znów słychać
}

State state() { return st; }
bool fmActive() { return false; }
bool hasMedia() { return false; }

}  // namespace audio
