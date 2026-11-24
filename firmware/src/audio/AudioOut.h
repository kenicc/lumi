#pragma once
// Tor audio: radio FM (RDA5807M, analogowo), radio internetowe i muzyka (I2S -> PCM5102A),
// dźwięki interfejsu i dzwonek (pliki WAV z LittleFS), wszystko na jeden wzmacniacz PAM8302.
// Gra tylko jedno źródło naraz; rozmowa wycisza wszystko i po rozłączeniu przywraca radio.
#include <stdint.h>

#include <string>
#include <vector>

namespace audio {

enum class Src : uint8_t { None, Fm, Stream };
enum class Sound : uint8_t { Key, Confirm, Error, AutoAnswer, Notice };

struct State {
  Src src = Src::None;
  bool playing = false;
  bool connecting = false;
  bool noWifi = false;  // strumień wymaga Wi-Fi
  bool failed = false;
  uint8_t volume = 5;  // 0..10
};

void begin(uint8_t startVolume);

void playFm(float mhz);
void playUrls(const std::vector<std::string>& urls);  // radio internetowe (1 adres) lub lista MP3
void stop();
void setVolume(uint8_t level);  // 0..10, aktywne źródło
void setMaxVolume(uint8_t level);

void sound(Sound s);
void setKeyClick(bool on);
void ring(bool on, uint8_t level);  // dzwonek narasta od cichszego do level
void callMode(bool on);             // rozmowa: media wyciszone, wzmacniacz włączony dla głosu z modemu

State state();
bool fmActive();  // dla sieci: w trybie FM Wi-Fi jest wyłączone (cisza elektryczna)

}  // namespace audio
