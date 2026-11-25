#include "app/App.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

#include <atomic>
#include <ArduinoJson.h>

#include <deque>
#include <map>
#include <string>

#include "app/Diag.h"
#include "app/SelfTest.h"
#include "app/State.h"
#include "audio/AudioOut.h"
#include "core/AtParse.h"
#include "core/Calendar.h"
#include "core/Pairing.h"
#include "core/ReminderQueue.h"
#include "hw/Battery.h"
#include "hw/Board.h"
#include "hw/Display.h"
#include "hw/Input.h"
#include "modem/Modem.h"
#include "net/Identity.h"
#include "net/Ota.h"
#include "net/Net.h"
#include "ui/Icons.h"
#include "ui/Ui.h"

LV_FONT_DECLARE(lumi_b20);
LV_FONT_DECLARE(lumi_r20);

namespace app {

namespace detail {

constexpr uint32_t POWER_LONG_MS = 3000;
constexpr uint32_t CONFIRM_TIMEOUT_MS = 10000;

cfg::Config C;
bool haveConfig = false;
char devId[24];

Screen scr = Screen::Home;
int contactIdx = 0, mediaIdx = 0, confirmIdx = -1;
int playingIdx = -1;  // pozycja listy mediów, która gra
uint32_t confirmAt = 0;

Call call;

Missed missed;

std::deque<std::string> notices;
std::string shownNotice;  // aktualnie widoczny - wraca do kolejki, gdy przerwie go rozmowa

// Przypomnienia od rodziny (logika w core/ReminderQueue - testowana na komputerze)
rem::Queue reminders;
Screen remPrev = Screen::Home;

// Wiadomości od rodziny: czekają do przeczytania, po wyjściu bez czytania wracają za 5 minut
std::deque<net::Message> inbox;
Screen msgPrev = Screen::Home;
uint32_t msgDeferUntil = 0;
constexpr uint32_t MSG_DEFER_MS = 5UL * 60 * 1000;
Preferences prefs;

uint32_t lastInput = 0;
bool screenOn = true;
uint8_t volume = 5, callVolume = 7;
int highlight = -1;
int lastMinute = -1;
uint32_t lastSecond = 0, lastTick = 0;
bool modemReady = false, simOk = true, modemFailed = false;
bool powerHolding = false;
uint32_t powerHoldAt = 0;
uint8_t batteryWarned = 100;
uint8_t ringOverride = 0;  // prototyp bez radia: rolka poza rozmową zmienia głośność dzwonka (0 = jak w aplikacji)
lv_obj_t* live = nullptr;
std::atomic<bool> idleFlag{false};
std::atomic<bool> callFlag{false};
bool healthyMarked = false;  // etykieta odświeżana co sekundę (czas rozmowy, odliczanie)

// ---------- pomocnicze ----------

bool mediaOn() { return C.media && audio::hasMedia(); }

uint8_t ringLevel() { return ringOverride ? ringOverride : C.ringVolume; }

const cfg::Contact* contactAt(int i) { return (i >= 0 && i < (int)C.contacts.size()) ? &C.contacts[i] : nullptr; }

bool timeValid() { return time(nullptr) > 1700000000; }

tm now_tm() {
  time_t t = time(nullptr);
  tm l;
  localtime_r(&t, &l);
  return l;
}

int minuteOfDay() {
  tm l = now_tm();
  return l.tm_hour * 60 + l.tm_min;
}

void applyBacklight() {
  if (screenOn)
    display::setBacklight(C.brightness, audio::fmActive());
  else
    display::off();
}

void wake() {
  lastInput = millis();
  if (!screenOn) {
    screenOn = true;
    applyBacklight();
  }
}

// ---------- działania ----------

int contactIndexByName(const std::string& name) {
  for (size_t i = 0; i < C.contacts.size(); i++)
    if (strcasecmp(C.contacts[i].name.c_str(), name.c_str()) == 0) return (int)i;
  return -1;
}

void startCall(const char* number, int contact);

void showMessage() {
  msgPrev = (scr == Screen::Confirm || scr == Screen::Message) ? Screen::Home : scr;
  scr = Screen::Message;
  wake();
  render();
  audio::sound(audio::Sound::Notice);
}

// "Przeczytane" (albo oddzwonienie): potwierdzenie dla rodziny i następna wiadomość
void readMessage(bool callBack) {
  if (inbox.empty()) return;
  net::Message m = inbox.front();
  inbox.pop_front();
  JsonDocument j;
  j["type"] = "message";
  j["id"] = m.id;
  j["status"] = "read";
  j["at"] = (long long)time(nullptr);
  std::string out;
  serializeJson(j, out);
  net::queueEvent(out);
  if (callBack) {
    int k = contactIndexByName(m.author);
    scr = msgPrev;
    if (k >= 0) startCall(C.contacts[k].number.c_str(), k);
    return;
  }
  if (inbox.empty()) scr = msgPrev;
  render();
}

void startCall(const char* number, int contact) {
  if (!modemReady) {
    ui::toast("Telefon jeszcze się uruchamia. Spróbuj za chwilę.");
    audio::sound(audio::Sound::Error);
    return;
  }
  if (!simOk) {
    ui::toast("Brak karty SIM");
    audio::sound(audio::Sound::Error);
    return;
  }
  call = Call{};
  call.phase = Phase::Outgoing;
  strlcpy(call.number, number, sizeof call.number);
  call.contact = contact;
  audio::callMode(true);  // radio cichnie, sygnał łączenia słychać z modemu
  modem::dial(number);
  render();
}

void playMedia(int idx) {
  if (idx < 0 || idx >= (int)C.mediaList.size()) return;
  const cfg::Media& m = C.mediaList[idx];
  playingIdx = idx;
  if (m.type == cfg::MediaType::Fm) audio::playFm(m.freq);
  else audio::playUrls(m.urls);
  prefs.putInt("media", idx);
}

void stopMedia() {
  audio::stop();
  playingIdx = -1;
}

void clearMissed() {
  missed = Missed{};
  prefs.remove("mNum");
  prefs.remove("mAt");
  prefs.remove("mCnt");
}

void silence() {
  if (call.phase != Phase::Incoming || call.silenced) return;
  call.silenced = true;
  audio::ring(false, 0);
  render();
}

// ---------- przypomnienia ----------

void reminderEvent(const rem::Item& d, const char* status) {
  JsonDocument j;
  j["type"] = "reminder";
  j["id"] = d.id;
  j["text"] = d.text;
  j["minute"] = d.minute;
  j["status"] = status;  // done / snoozed / missed / shown
  j["at"] = (long long)time(nullptr);
  std::string out;
  serializeJson(j, out);
  net::queueEvent(out);
}

void leaveReminder() {
  scr = remPrev;
  render();
}

void finishReminder(const char* status) {
  if (!reminders.active()) return;
  reminderEvent(reminders.current(), status);
  reminders.finish();
  leaveReminder();
}

void snoozeReminder() {
  rem::Item it = reminders.current();
  if (!reminders.snooze(millis())) return;
  reminderEvent(it, "snoozed");
  leaveReminder();
}

void startReadyReminder() {
  if (!reminders.start(millis())) return;
  remPrev = (scr == Screen::Confirm || scr == Screen::Reminder) ? Screen::Home : scr;
  scr = Screen::Reminder;
  if (ui::noticeVisible()) {  // przypomnienie ważniejsze niż komunikat o zmianie
    notices.push_front(shownNotice);
    ui::dismissNotice();
  }
  wake();
  render();
  audio::sound(audio::Sound::Notice);
}

void checkReminders(const tm& l) {
  if (!haveConfig || !timeValid()) return;
  reminders.addDue(C.reminders, l.tm_wday, l.tm_hour * 60 + l.tm_min);
}

// Prawa kolumna: znaczenie zależy od ekranu, ale góra = tak, dół = nie/wstecz tam, gdzie to możliwe
void rightAction(int slot) {
  if (!ui::railVisible(slot)) return;
  ui::flashRail(slot);
  audio::sound(audio::Sound::Key);

  if (call.phase == Phase::Incoming) {
    if (slot == 0) {
      audio::ring(false, 0);
      modem::answer();
    } else if (slot == 1) {
      silence();
    } else {
      call.rejected = true;
      audio::ring(false, 0);
      modem::hangup();
    }
    return;
  }
  if (call.phase != Phase::None) {
    if (slot == 2) {
      call.hungUpHere = true;
      modem::hangup();
    }
    return;
  }

  int nC = C.contacts.size(), nM = C.mediaList.size();
  switch (scr) {
    case Screen::Home:
      if (missed.has) {
        if (slot == 0) {
          char num[24];
          strlcpy(num, missed.number, sizeof num);
          int k = C.contactIndexByNumber(num);
          clearMissed();
          startCall(num, k);
        } else if (slot == 2) {
          clearMissed();
          render();
        }
        return;
      }
      switch (C.home[slot].kind) {
        case cfg::SlotKind::Contact: {
          const cfg::Contact* k = C.contactById(C.home[slot].contactId);
          if (!k) return;
          confirmIdx = k - &C.contacts[0];
          confirmAt = millis();
          go(Screen::Confirm);  // jedno naciśnięcie z kieszeni nie dzwoni
          break;
        }
        case cfg::SlotKind::Media:
          if (audio::state().playing) {
            mediaIdx = playingIdx >= 0 ? playingIdx : mediaIdx;
          } else {
            playMedia(mediaIdx);
          }
          go(Screen::Media);
          break;
        case cfg::SlotKind::Weather:
          net::refreshWeather();
          go(Screen::Weather);
          break;
        default: break;
      }
      break;
    case Screen::Contacts:
      if (slot == 0 && nC) contactIdx = (contactIdx + nC - 1) % nC;
      if (slot == 2 && nC) contactIdx = (contactIdx + 1) % nC;
      if (slot == 1 && nC) {
        startCall(C.contacts[contactIdx].number.c_str(), contactIdx);
        return;
      }
      render();
      break;
    case Screen::Media: {
      bool playing = audio::state().src != audio::Src::None;
      if ((slot == 0 || slot == 2) && nM) {
        mediaIdx = slot == 0 ? (mediaIdx + nM - 1) % nM : (mediaIdx + 1) % nM;
        if (playing) playMedia(mediaIdx);  // jak pokrętło w radiu: przełącza od razu
      } else if (slot == 1) {
        if (playing && playingIdx == mediaIdx) stopMedia();
        else playMedia(mediaIdx);
      }
      render();
      break;
    }
    case Screen::Weather:
      if (slot == 2) go(Screen::Home);
      break;
    case Screen::Confirm:
      if (slot == 0) {
        const cfg::Contact* k = contactAt(confirmIdx);
        scr = Screen::Home;
        if (k) startCall(k->number.c_str(), confirmIdx);
      } else if (slot == 2) {
        go(Screen::Home);
      }
      break;
    case Screen::Unpaired: break;
    case Screen::Message:
      if (slot == 0) {
        audio::sound(audio::Sound::Confirm);
        readMessage(false);
      } else if (slot == 1) {
        readMessage(true);
      }
      break;
    case Screen::Reminder:
      if (slot == 0) {
        audio::sound(audio::Sound::Confirm);
        finishReminder("done");
      } else if (slot == 2) {
        snoozeReminder();
        ui::toast("Przypomnę za 10 minut");
      }
      break;
    case Screen::Service:
      if (slot == 0) {
        net::startProvisioning();
        ui::toast("Sieć LUMI włączona na 5 minut");
      } else if (slot == 2) {
        go(haveConfig ? Screen::Home : Screen::Unpaired);
      }
      break;
  }
}

void shutdown() {
  esp_task_wdt_delete(nullptr);  // wyłączanie trwa kilka sekund - strażnik nie może przerwać
  ui::powerHold(false, 0, 0);
  ui::dismissNotice();
  ui::hideRails();
  ui::clearContent();
  lv_obj_t* l = ui::label(ui::content(), ui::fonts().title, ui::pal().text, "Do widzenia");
  lv_obj_center(l);
  audio::stop();
  modem::powerOff();
  uint32_t t = millis();
  while (millis() - t < 3500) {  // modem potrzebuje chwili na wyrejestrowanie z sieci
    display::tick();
    delay(10);
  }
  display::off();
  digitalWrite(pins::AMP_SD, LOW);
  while (digitalRead(pins::BTN_POWER) == LOW) delay(10);  // inaczej obudzi się od razu
  delay(200);
  rtc_gpio_pullup_en((gpio_num_t)pins::BTN_POWER);
  rtc_gpio_pulldown_dis((gpio_num_t)pins::BTN_POWER);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)pins::BTN_POWER, 0);
  esp_deep_sleep_start();
}

void onKey(Key k, Ev type) {
  if (selftest::active()) {
    lastInput = millis();
    selftest::onKey(k, type);
    if (!selftest::active()) render();  // koniec testu - zwykły ekran
    return;
  }
  bool wasOff = !screenOn;
  lastInput = millis();

  if (k == Key::Power) {
    if (type == Ev::KeyHoldStart) {
      powerHolding = true;
      powerHoldAt = millis();
      return;
    }
    if (type == Ev::KeyLong) {
      powerHolding = false;
      ui::powerHold(false, 0, 0);
      if (call.phase != Phase::None) return;
      if (C.allowPowerOff || !haveConfig) shutdown();
      wake();
      ui::toast("Wyłączanie zablokowała rodzina");
      return;
    }
    // krótkie naciśnięcie: wygaszenie / włączenie ekranu
    bool longish = powerHolding && millis() - powerHoldAt > 600;
    powerHolding = false;
    ui::powerHold(false, 0, 0);
    if (longish) return;  // puszczony w trakcie przytrzymania - tylko anuluj
    if (call.phase == Phase::Incoming || call.phase == Phase::Active) {
      wake();
      return;
    }
    if (screenOn) {
      screenOn = false;
      applyBacklight();
    } else {
      wake();
    }
    return;
  }

  // Wygaszony ekran: pierwsze naciśnięcie tylko budzi (nic się nie dzieje przypadkiem)
  if (wasOff && call.phase == Phase::None) {
    wake();
    return;
  }
  wake();

  if (ui::noticeVisible() && call.phase == Phase::None) {
    ui::dismissNotice();
    audio::sound(audio::Sound::Key);
    return;
  }

  bool right = k == Key::Top || k == Key::Mid || k == Key::Bottom;
  if (right) {
    int slot = (int)k;
    if (C.oneButton && k == Key::Mid) {
      if (type == Ev::KeyShort) {  // przejdź do następnej opcji
        for (int i = 1; i <= 3; i++) {
          int s = (highlight + i) % 3;
          if (ui::railVisible(s)) {
            highlight = s;
            break;
          }
        }
        ui::setHighlight(highlight);
        audio::sound(audio::Sound::Key);
        return;
      }
      if (type == Ev::KeyLong) {
        rightAction(highlight);
        return;
      }
      return;  // HoldStart
    }
    if (type == Ev::KeyShort) rightAction(slot);
    return;
  }
  if (k == Key::Home && type == Ev::KeyLong && call.phase == Phase::None) {
    audio::sound(audio::Sound::Confirm);
    go(Screen::Service);
    return;
  }
  if (type != Ev::KeyShort) return;

  // Dolny rząd: zawsze to samo, na każdym ekranie
  if (scr == Screen::Reminder && call.phase == Phase::None && reminders.active()) {
    // wyjście z przypomnienia bez potwierdzenia = przypomnij później (albo brak potwierdzenia)
    if (!reminders.current().confirm) finishReminder("shown");
    else if (reminders.canSnooze()) snoozeReminder();
    else finishReminder("missed");
  }
  if (scr == Screen::Message && call.phase == Phase::None) msgDeferUntil = millis() + MSG_DEFER_MS;  // wróci później
  if (call.phase != Phase::None) {
    ui::toast(call.phase == Phase::Incoming ? "Najpierw odbierz albo odrzuć" : "Najpierw rozłącz rozmowę");
    audio::sound(audio::Sound::Error);
    return;
  }
  audio::sound(audio::Sound::Key);
  switch (k) {
    case Key::Home:
      ui::dismissNotice();
      go(haveConfig ? Screen::Home : Screen::Unpaired);
      break;
    case Key::Music:
      if (!audio::hasMedia()) {
        ui::toast("Radio nie jest podłączone w tym prototypie");
        break;
      }
      if (!haveConfig || C.lockMusic || !C.media) {
        ui::toast("Ten przycisk jest wyłączony");
        break;
      }
      if (playingIdx >= 0) mediaIdx = playingIdx;
      go(Screen::Media);
      break;
    case Key::Call:
      if (!haveConfig || C.lockCall || !C.calls) {
        ui::toast("Ten przycisk jest wyłączony");
        break;
      }
      go(Screen::Contacts);
      break;
    default: break;
  }
}

void onWheel(int d) {
  if (selftest::active()) {
    selftest::onWheel(d);
    return;
  }
  wake();
  if (call.phase == Phase::Active || call.phase == Phase::Outgoing) {
    callVolume = clampv<int>(callVolume + d, 1, 10);
    modem::setCallVolume(callVolume);
    prefs.putUChar("callVol", callVolume);  // senior ustawia raz, zostaje na kolejne rozmowy
    ui::volumeBar("Rozmowa", callVolume, 10);
    return;
  }
  if (call.phase == Phase::Incoming) {
    if (d < 0) silence();  // skręcenie w dół wycisza dzwonek
    return;
  }
  if (!audio::hasMedia()) {  // prototyp bez radia: rolka ustawia dzwonek
    ringOverride = clampv<int>(ringLevel() + d, 1, 10);
    modem::ringer(ringOverride);
    ui::volumeBar("Dzwonek", ringOverride, 10);
    return;
  }
  volume = clampv<int>(volume + d, 0, C.maxVolume);
  audio::setVolume(volume);
  ui::volumeBar("Głośność", volume, C.maxVolume);
}

void onIncoming(const char* num) {
  if (selftest::active()) selftest::stop();  // rozmowa ważniejsza niż test
  int k = C.contactIndexByNumber(num);
  if (C.onlyContacts && k < 0) {
    modem::hangup();
    Serial.println("[rozmowa] odrzucono numer spoza listy kontaktów");
    return;
  }
  if (call.phase != Phase::None) return;
  if (ui::noticeVisible()) {  // rozmowa ma pierwszeństwo, komunikat wróci po niej
    notices.push_front(shownNotice);
    ui::dismissNotice();
  }
  ui::powerHold(false, 0, 0);
  powerHolding = false;
  call = Call{};
  call.phase = Phase::Incoming;
  strlcpy(call.number, num, sizeof call.number);
  call.contact = k;
  wake();
  bool quiet = timeValid() && C.inQuietHours(minuteOfDay());
  // W godzinach ciszy dzwonek jest cichy, ale nigdy wyłączony - rodzina musi się dodzwonić
  audio::ring(true, quiet ? (ringLevel() < 3 ? ringLevel() : 3) : ringLevel());
  if (k >= 0 && C.contacts[k].autoAnswer) call.autoAt = millis() + C.autoAnswerSec * 1000UL;
  render();
}

void onEnded(bool answered, const char* text) {
  Phase was = call.phase;
  if (was == Phase::None) return;
  if (was == Phase::Incoming && !answered && !call.rejected) {
    if (missed.has && at::sameNumber(missed.number, call.number)) {
      missed.count++;
    } else {
      missed = Missed{};
      missed.has = true;
      strlcpy(missed.number, call.number, sizeof missed.number);
      missed.count = 1;
    }
    missed.at = time(nullptr);
    prefs.putString("mNum", missed.number);
    prefs.putLong64("mAt", missed.at);
    prefs.putInt("mCnt", missed.count);
  }
  if (was == Phase::Outgoing && !answered && !call.hungUpHere)
    ui::toast(!strcmp(text, "error") ? "Nie udało się połączyć" : "Nikt nie odebrał");
  if (was == Phase::Active && !call.hungUpHere) ui::toast("Rozmowa zakończona");
  audio::ring(false, 0);
  audio::callMode(false);
  call = Call{};
  // przypomnienie przerwane rozmową wraca na ekran
  if (reminders.active()) scr = Screen::Reminder;
  else if (!(scr == Screen::Message && !inbox.empty())) scr = haveConfig ? Screen::Home : Screen::Unpaired;
  wake();
  render();
}

void announceChange() {
  if (C.changedBy.empty()) return;
  std::string t;
  if (!C.changeText.empty()) t = C.changedBy + " " + C.changeText;  // aplikacja zna rodzaj gramatyczny
  else t = "Ustawienia zmienione przez: " + C.changedBy;
  notices.push_back(t);
}

void applyConfig() {
  ui::setTheme(C.textSize, C.highContrast);
  input::configure(C.minPressMs, C.repeatIgnoreMs, C.oneButton);
  audio::setMaxVolume(C.maxVolume);
  audio::setKeyClick(C.keyClick);
  if (!audio::hasMedia()) modem::ringer(ringLevel());
  if (volume > C.maxVolume) {
    volume = C.maxVolume;
    audio::setVolume(volume);
  }
  net::Settings s;
  s.wifi = C.wifi;
  s.lat = C.lat;
  s.lon = C.lon;
  s.weather = C.weather;
  s.syncIntervalSec = C.syncIntervalSec;
  s.version = C.version;
  for (const auto& k : C.contacts)
    if (!k.photo.empty()) s.photos.push_back(k.photo);
  freeUnusedPhotos();
  net::apply(s);
  applyBacklight();
}

bool loadFile(const char* path, cfg::Config& out) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  std::string s;
  s.resize(f.size());
  f.read(reinterpret_cast<uint8_t*>(&s[0]), s.size());
  f.close();
  std::string err;
  if (!cfg::parse(s.data(), s.size(), out, err)) {
    diag::error("[config] %s: %s", path, err.c_str());
    return false;
  }
  return true;
}

bool loadConfig() {
  cfg::Config n;
  if (!loadFile("/config.json", n) && !loadFile("/config.bak", n)) return false;
  C = std::move(n);
  haveConfig = true;
  return true;
}

void checkBattery() {
  uint8_t p = battery::percent();
  if (battery::charging() || p > 30) {
    batteryWarned = 100;
    return;
  }
  uint8_t level = p <= 10 ? 10 : (p <= 20 ? 20 : 100);
  if (level < batteryWarned) {
    batteryWarned = level;
    char b[80];
    snprintf(b, sizeof b, "Bateria słaba (%u%%). Podłącz ładowarkę.", p);
    notices.push_back(b);
  }
}

void tick() {
  uint32_t now = millis();
  if (now - lastTick < 100) return;
  uint32_t dt = now - lastTick;
  lastTick = now;
  if (selftest::active()) {  // test sprzętu: bez wygaszania ekranu i bez zwykłych ekranów
    lastInput = now;
    selftest::tick();
    if (!selftest::active()) render();
    return;
  }
  rem::Queue::Tick rt = reminders.tick(now, call.phase != Phase::None, dt);

  if (powerHolding && (C.allowPowerOff || !haveConfig) && now - powerHoldAt > 300)
    ui::powerHold(true, now - powerHoldAt, POWER_LONG_MS);

  // automatyczne odbieranie: sygnał, potem otwarcie rozmowy
  if (call.phase == Phase::Incoming && call.autoAt && (int32_t)(now - call.autoAt) >= 0) {
    if (call.autoStage == 0) {
      call.autoStage = 1;
      call.autoAt = now + 1500;
      audio::ring(false, 0);
      audio::sound(audio::Sound::AutoAnswer);
    } else {
      call.autoAt = 0;
      call.autoAnswered = true;
      modem::answer();
    }
  }

  if (scr == Screen::Confirm && call.phase == Phase::None && now - confirmAt > CONFIRM_TIMEOUT_MS) go(Screen::Home);

  // przypomnienia: start, powtarzanie dźwięku co minutę, koniec bez potwierdzenia po 10 min
  if (call.phase == Phase::None && !reminders.active() && reminders.pending()) startReadyReminder();
  // wiadomość: po rozmowie i przypomnieniu, nie w trakcie parowania ani na ekranie serwisowym
  if (call.phase == Phase::None && !reminders.active() && !inbox.empty() && scr != Screen::Message &&
      scr != Screen::Unpaired && scr != Screen::Service && (int32_t)(now - msgDeferUntil) >= 0)
    showMessage();
  if (rt.beep) audio::sound(audio::Sound::Notice);
  if (rt.timeout != rem::Timeout::None) {
    // bez potwierdzania: po prostu "przypomniane"; z potwierdzaniem: rodzina dostanie powiadomienie
    reminderEvent(reminders.current(), rt.timeout == rem::Timeout::Missed ? "missed" : "shown");
    leaveReminder();
  }
  if (reminders.active() && call.phase == Phase::None) lastInput = now;  // ekran nie gaśnie, dopóki czeka

  if (screenOn && call.phase == Phase::None && now - lastInput > C.screenOffSec * 1000UL) {
    screenOn = false;
    applyBacklight();
    ui::dismissNotice();
    // po wybudzeniu zawsze zegar - chyba że czeka wiadomość, wtedy ona
    if (scr != Screen::Unpaired && !(scr == Screen::Message && !inbox.empty())) scr = Screen::Home;
    render();
  }

  if (!ui::noticeVisible() && call.phase == Phase::None && !notices.empty()) {
    wake();
    shownNotice = notices.front();
    notices.pop_front();
    ui::notice(IC_INFO, shownNotice.c_str());
    audio::sound(audio::Sound::Notice);
  }

  idleFlag = !screenOn && call.phase == Phase::None && !audio::state().playing;
  callFlag = call.phase != Phase::None;
  if (!healthyMarked && now > 180000) {  // 3 minuty pracy: nowa wersja oprogramowania jest zdrowa
    healthyMarked = true;
    ota::markHealthy();
  }

  if (now - lastSecond >= 1000) {
    lastSecond = now;
    updateLive();
    checkBattery();
    tm l = now_tm();
    if (l.tm_min != lastMinute) {
      lastMinute = l.tm_min;
      checkReminders(l);
      if (scr == Screen::Home && call.phase == Phase::None) render();
    }
  }
}

void handle(const Event& e) {
  switch (e.type) {
    case Ev::KeyShort:
    case Ev::KeyHoldStart:
    case Ev::KeyLong: onKey((Key)e.a, e.type); break;
    case Ev::Wheel: onWheel(e.a); break;
    case Ev::ModemReady:
      modemReady = true;
      modemFailed = false;
      simOk = e.a == 1;
      if (call.phase == Phase::None) render();
      break;
    case Ev::ModemFail:
      modemFailed = true;
      if (call.phase == Phase::None) render();
      break;
    case Ev::NetTime:
    case Ev::WeatherUpdated:
      if (call.phase == Phase::None && (scr == Screen::Home || scr == Screen::Weather)) render();
      break;
    case Ev::CallIncoming: onIncoming(e.text); break;
    case Ev::CallDialing: break;
    case Ev::CallActive:
      if (call.phase == Phase::Incoming || call.phase == Phase::Outgoing) {
        call.phase = Phase::Active;
        call.startedAt = millis();
        audio::callMode(true);
        modem::setCallVolume(callVolume);
        render();
      }
      break;
    case Ev::CallEnded: onEnded(e.a == 1, e.text); break;
    case Ev::ConfigUpdated: reloadConfig(true); break;
    case Ev::MessagesArrived:
      for (auto& m : net::takeMessages()) {
        bool dup = false;
        for (auto& x : inbox) dup |= x.id == m.id;
        if (!dup) inbox.push_back(m);
      }
      msgDeferUntil = 0;  // nowa wiadomość: pokaż od razu
      break;
    case Ev::PhotosUpdated:
      dropMissingPhotos();
      if (call.phase != Phase::None || scr == Screen::Contacts || scr == Screen::Confirm) render();
      break;
    case Ev::CloudState:
      if (call.phase == Phase::None && (scr == Screen::Unpaired || scr == Screen::Service)) render();
      break;
    case Ev::MediaState:
      applyBacklight();  // FM -> podświetlenie bez PWM
      if (call.phase == Phase::None && (scr == Screen::Media || scr == Screen::Home)) render();
      break;
  }
}

}  // namespace detail

using namespace detail;

// ---------- interfejs publiczny ----------

void begin() {
  identity::begin();
  strlcpy(devId, identity::id(), sizeof devId);
  prefs.begin("lumi", false);
  callVolume = prefs.getUChar("callVol", 7);
  ui::begin();
  bool ok = loadConfig();
  if (!ok) Serial.println("[config] brak konfiguracji - ekran parowania");
  volume = C.startVolume;

  if (prefs.isKey("mNum")) {
    missed.has = true;
    strlcpy(missed.number, prefs.getString("mNum", "").c_str(), sizeof missed.number);
    missed.at = prefs.getLong64("mAt", 0);
    missed.count = prefs.getInt("mCnt", 1);
  }
  mediaIdx = prefs.getInt("media", 0);

  audio::begin(volume);
  modem::begin();
  net::begin();
  applyConfig();
  scr = ok ? Screen::Home : Screen::Unpaired;
  bool startTest = digitalRead(pins::BTN_HOME) == LOW;  // Home przytrzymany przy włączaniu = test sprzętu
  lastInput = millis();
  // zasada 5: użytkownik wie, że coś się zmieniło
  if (ota::updatedThisBoot()) notices.push_back("LUMI ma nową wersję oprogramowania");
  if (startTest) {
    render();
    selftest::start();
    return;
  }
  render();
}

void loop() {
  Event e;
  while (bus::receive(e, 0)) handle(e);
  tick();
}

const cfg::Config& config() { return C; }
bool configured() { return haveConfig; }
const char* deviceId() { return devId; }

bool reloadConfig(bool announce) {
  int oldVersion = haveConfig ? C.version : -1;
  if (!loadConfig()) return false;
  applyConfig();
  if (announce && C.version > oldVersion) announceChange();
  if (scr == Screen::Unpaired) scr = Screen::Home;
  if (call.phase == Phase::None) render();
  return true;
}

void showNotice(const char* text) { notices.push_back(text); }

bool idleForUpdate() { return idleFlag; }

void testMessage(const char* author, const char* text) {
  static int id = 900000;
  inbox.push_back({++id, author, text, (long long)time(nullptr)});
}

void startSelfTest() {
  selftest::start();
}

void testReminder(const char* text) {
  tm l = now_tm();
  reminders.add({"test", text, l.tm_hour * 60 + l.tm_min, true, 0, 0});
}
bool inCall() { return callFlag; }

}  // namespace app
