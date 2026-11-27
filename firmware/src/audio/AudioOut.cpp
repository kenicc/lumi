#include "../BuildConfig.h"
// Pełny tor dźwięku (DAC, radio FM). Wariant allegro używa AudioModem.cpp - ten plik jest wtedy pusty,
// żeby Arduino IDE (kompiluje wszystkie pliki) nie dublowało funkcji ani nie szukało bibliotek audio.
#ifndef LUMI_AUDIO_MODEM
#include "AudioOut.h"

#include <Arduino.h>
#include <Audio.h>
#include <LittleFS.h>
#include <RDA5807.h>
#include <WiFi.h>
#include <Wire.h>

#include "../app/Events.h"
#include "../hw/Board.h"

namespace audio {

namespace {

enum class CmdType : uint8_t { Fm, Urls, Stop, Volume, MaxVolume, Sound, Ring, RingOff, CallOn, CallOff, KeyClick };
struct Cmd {
  CmdType t;
  float f = 0;
  uint8_t v = 0;
  std::vector<std::string>* urls = nullptr;  // własność przechodzi na zadanie audio
};

QueueHandle_t q;
SemaphoreHandle_t mtx;
State st;

Audio* player = nullptr;
RDA5807 fm;
bool fmPowered = false;

// media
Src src = Src::None;
float fmFreq = 0;
std::vector<std::string> urls;
size_t urlIdx = 0;
bool streamOn = false;      // użytkownik chce słuchać strumienia
bool streamRunning = false;  // dekoder gra
uint32_t connectAt = 0;
uint8_t retries = 0;

// dźwięki i dzwonek
bool sfxPlaying = false;
bool ringing = false;
uint8_t ringLevel = 8, ringCur = 0;
bool inCall = false;
bool keyClick = true;
volatile bool eof = false;

uint8_t volume = 5, maxVolume = 10;
uint32_t ampOffAt = 0;

constexpr uint8_t SFX_LEVEL = 4;

void publish() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  st.src = src;
  st.playing = (src == Src::Fm && !inCall) || (src == Src::Stream && streamRunning);
  st.volume = volume;
  xSemaphoreGive(mtx);
  bus::post(Event(Ev::MediaState));
}

void setFlags(bool connecting, bool noWifi, bool failed) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  st.connecting = connecting;
  st.noWifi = noWifi;
  st.failed = failed;
  xSemaphoreGive(mtx);
}

uint8_t toStream(uint8_t lvl) { return (uint8_t)((lvl * 21 + 5) / 10); }  // 0..21
uint8_t toFm(uint8_t lvl) { return (uint8_t)((lvl * 15 + 5) / 10); }      // 0..15

void amp(bool on) {
  if (on) {
    digitalWrite(pins::AMP_SD, HIGH);
    ampOffAt = 0;
  } else if (!ampOffAt) {
    ampOffAt = millis() + 400;  // chwila zwłoki, żeby nie trzaskało między dźwiękami
  }
}

void fmPower(bool on) {
  if (on && !fmPowered) {
    fm.powerUp();
    fm.setMono(true);  // mały głośnik mono: mniej szumu przy słabym sygnale
    fmPowered = true;
  } else if (!on && fmPowered) {
    fm.setMute(true);
    fm.powerDown();
    fmPowered = false;
  }
}

void applyFmVolume() {
  if (!fmPowered) return;
  uint8_t v = toFm(volume);
  fm.setMute(v == 0 || inCall || ringing);
  if (v) fm.setVolume(v);
}

void stopDecoder() {
  if (player->isRunning()) player->stopSong();
  streamRunning = false;
  sfxPlaying = false;
}

bool startUrl() {
  if (urls.empty()) return false;
  if (WiFi.status() != WL_CONNECTED) {
    setFlags(true, true, false);
    return false;
  }
  setFlags(true, false, false);
  player->setVolume(toStream(volume));
  bool ok = player->connecttohost(urls[urlIdx].c_str());
  streamRunning = ok;
  if (ok) setFlags(false, false, false);
  return ok;
}

void playFile(const char* path, uint8_t level) {
  if (streamRunning) return;  // nie przerywamy radia internetowego kliknięciem
  if (!LittleFS.exists(path)) return;
  player->setVolume(toStream(level));
  sfxPlaying = player->connecttoFS(LittleFS, path);
  if (sfxPlaying) amp(true);
}

void resumeMedia() {
  if (src == Src::Fm) {
    applyFmVolume();
  } else if (src == Src::Stream && streamOn) {
    retries = 0;
    connectAt = millis();
  }
}

void handle(Cmd& c) {
  switch (c.t) {
    case CmdType::Fm:
      stopDecoder();
      streamOn = false;
      src = Src::Fm;
      fmFreq = c.f;
      fmPower(true);
      fm.setFrequency((uint16_t)(c.f * 100 + 0.5f));
      applyFmVolume();
      setFlags(false, false, false);
      break;
    case CmdType::Urls:
      fmPower(false);
      stopDecoder();
      urls = std::move(*c.urls);
      delete c.urls;
      urlIdx = 0;
      src = Src::Stream;
      streamOn = true;
      retries = 0;
      connectAt = millis();
      break;
    case CmdType::Stop:
      fmPower(false);
      stopDecoder();
      streamOn = false;
      src = Src::None;
      setFlags(false, false, false);
      break;
    case CmdType::Volume:
      volume = c.v > maxVolume ? maxVolume : c.v;
      applyFmVolume();
      if (streamRunning) player->setVolume(toStream(volume));
      break;
    case CmdType::MaxVolume:
      maxVolume = c.v;
      if (volume > maxVolume) volume = maxVolume;
      break;
    case CmdType::Sound: {
      static const char* const kFiles[] = {"/sounds/key.wav", "/sounds/ok.wav", "/sounds/error.wav",
                                           "/sounds/autoanswer.wav", "/sounds/notice.wav"};
      Sound s = (Sound)c.v;
      if (s == Sound::Key && !keyClick) break;
      if (ringing || inCall) break;
      playFile(kFiles[(int)s], s == Sound::AutoAnswer ? 8 : SFX_LEVEL);
      break;
    }
    case CmdType::KeyClick: keyClick = c.v; break;
    case CmdType::Ring:
      if (streamRunning) stopDecoder();  // radio wróci po rozmowie
      ringing = true;
      ringLevel = c.v;
      ringCur = ringLevel > 3 ? ringLevel - 3 : 1;  // zaczyna ciszej i narasta
      applyFmVolume();
      playFile("/sounds/ring.wav", ringCur);
      break;
    case CmdType::RingOff:
      ringing = false;
      if (sfxPlaying) stopDecoder();
      if (!inCall) resumeMedia();
      break;
    case CmdType::CallOn:
      ringing = false;
      inCall = true;
      stopDecoder();
      applyFmVolume();
      amp(true);  // głos rozmówcy z modemu idzie przez mikser na ten sam wzmacniacz
      break;
    case CmdType::CallOff:
      inCall = false;
      resumeMedia();
      break;
  }
  publish();
}

void onEof() {
  if (ringing) {
    if (ringCur < ringLevel) ringCur++;
    playFile("/sounds/ring.wav", ringCur);
    return;
  }
  if (sfxPlaying) {
    sfxPlaying = false;
    return;
  }
  if (src == Src::Stream && streamOn) {
    streamRunning = false;
    if (urls.size() > 1) urlIdx = (urlIdx + 1) % urls.size();  // następny utwór z listy
    connectAt = millis() + (urls.size() > 1 ? 0 : 2000);       // radio: spróbuj ponownie
    publish();
  }
}

void task(void*) {
  for (;;) {
    Cmd c;
    while (xQueueReceive(q, &c, 0) == pdTRUE) handle(c);

    if (player->isRunning()) player->loop();

    if (eof) {
      eof = false;
      onEof();
    }

    // (ponowne) łączenie ze strumieniem
    if (src == Src::Stream && streamOn && !streamRunning && !inCall && !ringing && connectAt &&
        (int32_t)(millis() - connectAt) >= 0) {
      connectAt = 0;
      if (!startUrl()) {
        if (++retries <= 5) {
          connectAt = millis() + 3000;
        } else {
          streamOn = false;
          setFlags(false, WiFi.status() != WL_CONNECTED, true);
        }
      } else {
        retries = 0;
      }
      publish();
    }

    bool needAmp = inCall || ringing || sfxPlaying || streamRunning || (src == Src::Fm && volume > 0);
    amp(needAmp);
    if (!needAmp && ampOffAt && (int32_t)(millis() - ampOffAt) >= 0) {
      digitalWrite(pins::AMP_SD, LOW);
      ampOffAt = 0;
    }
    vTaskDelay(player->isRunning() ? 1 : 10);
  }
}

void send(CmdType t, uint8_t v = 0, float f = 0, std::vector<std::string>* u = nullptr) {
  Cmd c;
  c.t = t;
  c.v = v;
  c.f = f;
  c.urls = u;
  if (xQueueSend(q, &c, pdMS_TO_TICKS(100)) != pdTRUE) delete u;
}

}  // namespace

void begin(uint8_t startVolume) {
  pinMode(pins::AMP_SD, OUTPUT);
  digitalWrite(pins::AMP_SD, LOW);
  volume = startVolume;
  q = xQueueCreate(12, sizeof(Cmd));
  mtx = xSemaphoreCreateMutex();

  Wire.begin(pins::I2C_SDA, pins::I2C_SCL);
  fm.setup();
  fm.setMute(true);
  fm.powerDown();

  player = new Audio(false, 3, I2S_NUM_0);
  player->setPinout(pins::I2S_BCLK, pins::I2S_LRCK, pins::I2S_DOUT);
  player->setVolume(toStream(volume));

  xTaskCreatePinnedToCore(task, "audio", 8192, nullptr, 4, nullptr, 1);
}

void playFm(float mhz) { send(CmdType::Fm, 0, mhz); }
void playUrls(const std::vector<std::string>& u) { send(CmdType::Urls, 0, 0, new std::vector<std::string>(u)); }
void stop() { send(CmdType::Stop); }
void setVolume(uint8_t level) { send(CmdType::Volume, level); }
void setMaxVolume(uint8_t level) { send(CmdType::MaxVolume, level); }
void sound(Sound s) { send(CmdType::Sound, (uint8_t)s); }
void setKeyClick(bool on) { send(CmdType::KeyClick, on); }
void ring(bool on, uint8_t level) { send(on ? CmdType::Ring : CmdType::RingOff, level); }
void callMode(bool on) { send(on ? CmdType::CallOn : CmdType::CallOff); }

State state() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  State s = st;
  xSemaphoreGive(mtx);
  return s;
}

bool fmActive() { return state().src == Src::Fm; }
bool hasMedia() { return true; }

}  // namespace audio

// Funkcje zwrotne biblioteki ESP32-audioI2S (wywoływane z zadania audio)
void audio_eof_mp3(const char*) { audio::eof = true; }
void audio_eof_stream(const char*) { audio::eof = true; }

#endif
