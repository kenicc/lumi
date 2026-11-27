// Symulator ekranu LUMI: kompiluje prawdziwy kod ekranów (src/app/App.cpp, src/ui) z LVGL
// na komputerze, odgrywa scenariusze i zapisuje zrzuty ekranu (PPM -> PNG w render.py).
// Sprzęt (modem, audio, sieć) jest zastąpiony prostymi atrapami poniżej.
#include <lvgl.h>
#include <sys/stat.h>

#include <deque>
#include <string>

#include "Arduino.h"
#include "LittleFS.h"
#include "app/App.h"
#include "app/SelfTest.h"
#include "app/State.h"
#include "ui/Ui.h"
#include "app/Events.h"
#include "audio/AudioOut.h"
#include "hw/Battery.h"
#include "hw/Display.h"
#include "hw/Input.h"
#include "modem/Modem.h"
#include "net/Identity.h"
#include "net/Net.h"

// ---------- atrapy Arduino ----------
SerialStub Serial;
EspStub ESP;
LittleFSStub LittleFS;
static uint32_t simMs = 1000;
static time_t simNow = 0;
time_t sim_time(time_t* t) {
  if (t) *t = simNow;
  return simNow;
}
uint32_t millis() { return simMs; }
void delay(uint32_t ms) { simMs += ms; }
int digitalRead(int) { return HIGH; }
void digitalWrite(int, int) {}

// ---------- atrapy modułów ----------
static std::deque<Event> q;
namespace bus {
void begin() {}
bool post(const Event& e) {
  q.push_back(e);
  return true;
}
bool receive(Event& e, uint32_t) {
  if (q.empty()) return false;
  e = q.front();
  q.pop_front();
  return true;
}
}  // namespace bus

static audio::State aState;
static bool simHasMedia = true;
namespace audio {
void begin(uint8_t v) { aState.volume = v; }
void playFm(float) {
  aState.src = Src::Fm;
  aState.playing = true;
  bus::post(Event(Ev::MediaState));
}
void playUrls(const std::vector<std::string>&) {
  aState.src = Src::Stream;
  aState.playing = true;
  bus::post(Event(Ev::MediaState));
}
void stop() {
  aState = State{};
  bus::post(Event(Ev::MediaState));
}
void setVolume(uint8_t l) { aState.volume = l; }
void setMaxVolume(uint8_t) {}
void sound(Sound) {}
void setKeyClick(bool) {}
void ring(bool, uint8_t) {}
void callMode(bool) {}
State state() { return aState; }
bool fmActive() { return aState.src == Src::Fm; }
bool hasMedia() { return simHasMedia; }
}  // namespace audio

namespace modem {
void begin() {}
void dial(const char*) { bus::post(Event(Ev::CallDialing)); }
void answer() { bus::post(Event(Ev::CallActive)); }
void hangup() { bus::post(Event(Ev::CallEnded, 0)); }
void setCallVolume(uint8_t) {}
void powerOff() {}
void ringer(uint8_t) {}
void tone(uint16_t, uint16_t, uint16_t, uint16_t) {}
void raw(const char*) {}
void diag() {}
void bridge(Stream&) {}
static int simBars = 4;
Status status() {
  Status s;
  s.ready = s.simOk = s.registered = s.lte = true;
  s.bars = simBars;
  return s;
}
const char* vendorName(Vendor) { return "sim"; }
}  // namespace modem

static wx::Forecast fc;
static net::Cloud cloudSim = net::Cloud::Provisioning;
namespace net {
void begin() {}
void apply(const Settings&) {}
static bool simWifi = true;
bool wifiConnected() { return simWifi; }
std::string wifiSsid() { return "Dom"; }
Cloud cloud() { return cloudSim; }
const char* cloudName(Cloud) { return ""; }
void reportSoon() {}
void startProvisioning() {}
static std::vector<std::string> events;  // zdarzenia wysłane "do rodziny"
void queueEvent(const std::string& j) {
  printf("zdarzenie: %s\n", j.c_str());
  events.push_back(j);
}
std::string photoPath(const std::string& h) { return "/ph/" + h + ".565"; }
std::vector<Message> takeMessages() { return {}; }
wx::Forecast forecast() { return fc; }
void refreshWeather() {}
bool saveConfigFile(const char*, size_t) { return true; }
}  // namespace net

namespace identity {
void begin() {}
const char* id() { return "LUMI-C3D4E5"; }
const char* secret() { return "0"; }
const char* pairCode() { return "K7MPQ2XR"; }
const char* apPassword() { return "40719263"; }
std::string server() { return "https://lumi-rodzina.vercel.app"; }
void setServer(const std::string&) {}
std::vector<cfg::Wifi> wifi() { return {}; }
void addWifi(const std::string&, const std::string&) {}
void factoryReset() {}
}  // namespace identity

namespace diag {
void error(const char* fmt, ...) { (void)fmt; }
}  // namespace diag
namespace ota {
void markHealthy() {}
bool updatedThisBoot() { return false; }
}  // namespace ota

static uint8_t batPct = 76;
namespace battery {
void begin() {}
void poll() {}
uint16_t millivolts() { return 3900; }
uint8_t percent() { return batPct; }
bool charging() { return false; }
bool known() { return true; }
}  // namespace battery

namespace input {
void begin() {}
void poll() {}
void configure(uint16_t, uint16_t, bool) {}
void inject(Key k, BtnEvent e) {
  bus::post(Event(e == BtnEvent::Short ? Ev::KeyShort : e == BtnEvent::Long ? Ev::KeyLong : Ev::KeyHoldStart,
                  (int32_t)k));
}
const char* keyName(Key) { return ""; }
bool keyFromName(const char*, Key&) { return false; }
}  // namespace input

// ---------- ekran ----------
static uint16_t fb[320 * 240];
static void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  uint16_t* p = (uint16_t*)px;
  for (int y = a->y1; y <= a->y2; y++)
    for (int x = a->x1; x <= a->x2; x++) fb[y * 320 + x] = *p++;
  lv_display_flush_ready(d);
}
static uint32_t tickCb() { return simMs; }
namespace display {
void begin() {
  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t* d = lv_display_create(320, 240);
  static uint16_t buf[320 * 240];
  lv_display_set_buffers(d, buf, nullptr, sizeof buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(d, flush);
}
void tick() { lv_timer_handler(); }
void setBacklight(uint8_t, bool) {}
void off() {}
bool isOn() { return true; }
}  // namespace display

// ---------- scenariusze ----------
static void run(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) {
    simMs += 10;
    app::loop();
    lv_timer_handler();
  }
}

// ---------- asercje zachowania ----------
static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) {
    failures++;
    printf("BLAD: %s\n", what);
  }
}
static bool lastEventHas(const char* s) { return !net::events.empty() && net::events.back().find(s) != std::string::npos; }
using app::detail::Phase;
using app::detail::Screen;
namespace D = app::detail;

static void shot(const char* name) {
  run(300);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  std::string path = std::string("sim/out/") + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb");
  fprintf(f, "P6\n320 240\n255\n");
  for (int i = 0; i < 320 * 240; i++) {
    uint16_t c = fb[i];
    uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 0x1F) * 255 / 31), (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                      (uint8_t)((c & 0x1F) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("zrzut: %s\n", path.c_str());
}

static void key(Key k, Ev t = Ev::KeyShort) {
  bus::post(Event(t, (int32_t)k));
  run(200);
}

static void writeCfg(const char* dir, const std::string& json) {
  mkdir(dir, 0755);
  FILE* f = fopen((std::string(dir) + "/config.json").c_str(), "w");
  fputs(json.c_str(), f);
  fclose(f);
}

static std::string readFile(const char* p) {
  FILE* f = fopen(p, "r");
  std::string s;
  char b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
  fclose(f);
  return s;
}

static void replace(std::string& s, const std::string& a, const std::string& b) {
  size_t p = s.find(a);
  if (p != std::string::npos) s.replace(p, a.size(), b);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  simNow = 1795478400 + 17 * 3600 + 40 * 60;  // wtorek 24.11.2026, 18:40 czasu polskiego
  mkdir("sim/out", 0755);

  fc.valid = true;
  fc.tempNow = 6.3f;
  fc.codeNow = 3;
  fc.today = {true, 3, 2, 7};
  fc.tomorrow = {true, 61, 1.5f, 8};

  // 1. Pierwsze uruchomienie, bez konfiguracji
  LittleFS.root = "sim/out/empty";
  mkdir("sim/out/empty", 0755);
  display::begin();
  app::begin();
  shot("00_parowanie_wifi");
  check(D::scr == Screen::Unpaired, "bez konfiguracji: ekran parowania");
  cloudSim = net::Cloud::Unclaimed;
  bus::post(Event(Ev::CloudState, (int32_t)cloudSim));
  shot("00_parowanie_aplikacja");
  cloudSim = net::Cloud::Ok;

  // 2. Konfiguracja przykładowa
  std::string base = readFile("data/config.json");
  // zdjęcie Ani przygotowane tym samym potokiem co na serwerze (sim/build.sh -> sim/out/cfg/ph)
  replace(base, "\"color\": \"#3D8BFD\", \"autoAnswer\": true", "\"color\": \"#3D8BFD\", \"autoAnswer\": true, \"photo\": \"0123456789abcdef\"");
  writeCfg("sim/out/cfg", base);
  LittleFS.root = "sim/out/cfg";
  app::reloadConfig(false);
  bus::post(Event(Ev::ModemReady, 1));
  shot("01_ekran_glowny");
  check(D::scr == Screen::Home, "po konfiguracji: ekran główny");
  // bez Wi-Fi: ikona połączenia pokazuje kreski zasięgu komórkowego
  net::simWifi = false;
  modem::simBars = 2;
  bus::post(Event(Ev::NetTime));
  shot("01b_bez_wifi_zasieg_komorkowy");
  net::simWifi = true;
  modem::simBars = 4;
  bus::post(Event(Ev::NetTime));

  key(Key::Call);
  shot("02_kontakty");
  key(Key::Bottom);
  shot("03_kontakty_nastepny");
  check(D::scr == Screen::Contacts && D::contactIdx == 1, "Następny przewija kontakty");

  key(Key::Music);
  key(Key::Mid);
  shot("04_radio_gra");

  key(Key::Home);
  key(Key::Mid);  // bez etykiet: środkowy boczny przycisk na zegarze = pogoda
  shot("05_pogoda");
  check(D::scr == Screen::Weather, "srodkowy boczny na zegarze pokazuje pogode");

  // szybkie wybieranie z zegara istnieje tylko z etykietami (tryb jednego przycisku, starsze warianty)
  key(Key::Home);
  ui::setRailLabels(true);
  D::render();
  key(Key::Top);
  shot("06_potwierdzenie");
  check(D::scr == Screen::Confirm, "szybkie wybieranie pyta o potwierdzenie");
  key(Key::Bottom);
  ui::setRailLabels(false);
  D::render();

  bus::post(Event(Ev::CallIncoming, 0, 0, "+48600100200"));
  run(1200);
  shot("07_rozmowa_przychodzaca_auto");
  check(D::call.phase == Phase::Incoming && D::call.contact == 0 && D::call.autoAt != 0, "Ania dzwoni, auto-odbieranie zaplanowane");
  key(Key::Bottom);  // odrzuć

  bus::post(Event(Ev::CallIncoming, 0, 0, "+48601200300"));
  run(500);
  shot("08_rozmowa_przychodzaca");
  check(!D::missed.has, "odrzucona rozmowa nie jest nieodebrana");
  key(Key::Top);
  run(65000);
  shot("09_rozmowa_trwa");
  check(D::call.phase == Phase::Active, "po Odbierz rozmowa trwa");
  key(Key::Bottom);
  run(500);

  bus::post(Event(Ev::CallIncoming, 0, 0, "+48512345678"));
  run(500);
  bus::post(Event(Ev::CallEnded, 0));
  run(500);
  shot("10_nieodebrane");
  check(D::missed.has && !strcmp(D::missed.number, "+48512345678"), "nieodebrana rozmowa zapamiętana");
  key(Key::Bottom);

  bus::post(Event(Ev::Wheel, 2));
  run(200);
  shot("11_glosnosc");
  run(2500);

  // zmiana od rodziny
  std::string v2 = base;
  replace(v2, "\"version\": 1", "\"version\": 2");
  replace(v2, "\"changedBy\": \"\"", "\"changedBy\": \"Ania\"");
  replace(v2, "\"changeText\": \"\"", "\"changeText\": \"dodała kontakt Basia\"");
  writeCfg("sim/out/cfg", v2);
  app::reloadConfig(true);
  run(500);
  shot("12_komunikat_zmiany");
  check(ui::noticeVisible(), "zmiana od rodziny pokazuje komunikat");
  key(Key::Top);

  // bardzo duży tekst i wysoki kontrast
  std::string big = v2;
  replace(big, "\"textSize\": 0", "\"textSize\": 2");
  replace(big, "\"highContrast\": false", "\"highContrast\": true");
  writeCfg("sim/out/cfg", big);
  app::reloadConfig(false);
  key(Key::Home);
  shot("13_duzy_kontrast_glowny");
  bus::post(Event(Ev::CallIncoming, 0, 0, "+48601200300"));
  run(500);
  shot("14_duzy_kontrast_rozmowa");
  key(Key::Bottom);
  run(500);
  key(Key::Call);
  shot("15_duzy_kontrast_kontakty");

  // tryb jednego przycisku
  std::string one = base;
  replace(one, "\"oneButton\": false", "\"oneButton\": true");
  replace(one, "\"version\": 1", "\"version\": 3");
  writeCfg("sim/out/cfg", one);
  app::reloadConfig(false);
  key(Key::Home);
  key(Key::Mid);
  shot("16_jeden_przycisk");

  // niska bateria
  batPct = 15;
  run(1500);
  shot("17_bateria");
  key(Key::Top);
  batPct = 76;

  // prototyp z Allegro: bez radia, rolka = dzwonek
  simHasMedia = false;
  one = base;
  replace(one, "\"version\": 1", "\"version\": 4");
  writeCfg("sim/out/cfg", one);
  app::reloadConfig(false);
  key(Key::Home);
  shot("18_allegro_glowny");
  bus::post(Event(Ev::Wheel, 1));
  run(200);
  shot("19_allegro_dzwonek");
  run(2500);

  // ekran serwisowy (Home 5 s)
  key(Key::Home, Ev::KeyLong);
  shot("20_serwis");
  cloudSim = net::Cloud::Unclaimed;
  bus::post(Event(Ev::CloudState, (int32_t)cloudSim));
  shot("21_serwis_parowanie");
  key(Key::Bottom);

  // przypomnienie od rodziny
  app::testReminder("Weź leki na ciśnienie");
  run(500);
  shot("22_przypomnienie");
  check(D::scr == Screen::Reminder && D::reminders.active(), "przypomnienie na ekranie");
  bus::post(Event(Ev::CallIncoming, 0, 0, "+48601200300"));
  run(500);
  bus::post(Event(Ev::CallEnded, 1));
  run(500);
  shot("23_przypomnienie_po_rozmowie");
  check(D::scr == Screen::Reminder, "przypomnienie wraca po rozmowie");
  key(Key::Top);
  shot("24_po_potwierdzeniu");
  check(lastEventHas("\"status\":\"done\"") && !D::reminders.active(), "Zrobione wysyła potwierdzenie do rodziny");

  // wiadomość od rodziny
  app::testMessage("Ania", "Przyjadę o 15, nie gotuj obiadu");
  run(500);
  shot("25_wiadomosc");
  check(D::scr == Screen::Message && D::inbox.size() == 1, "wiadomość na ekranie");
  key(Key::Top);
  shot("26_po_przeczytaniu");
  check(lastEventHas("\"type\":\"message\"") && D::inbox.empty(), "Dobrze potwierdza przeczytanie");

  // test sprzętu krok po kroku
  app::startSelfTest();
  run(300);
  shot("27_test_ekran");
  key(Key::Top);
  key(Key::Top);
  key(Key::Music);
  key(Key::Home);
  shot("28_test_przyciski");
  key(Key::Mid);
  key(Key::Bottom);
  key(Key::Call);
  key(Key::Power);
  bus::post(Event(Ev::Wheel, 3));
  run(300);
  bus::post(Event(Ev::Wheel, -3));
  run(1500);  // modem w atrapie od razu zalogowany
  key(Key::Top);  // dźwięk słychać
  key(Key::Top);  // bateria
  key(Key::Top);  // sieć
  shot("29_test_wynik");
  key(Key::Home);
  shot("30_po_tescie");

  // test z błędami: zły obraz, rolka odwrotnie, brak dźwięku
  app::startSelfTest();
  run(300);
  key(Key::Bottom);
  for (Key k : {Key::Top, Key::Mid, Key::Bottom, Key::Music, Key::Home, Key::Call, Key::Power}) key(k);
  bus::post(Event(Ev::Wheel, -3));
  run(1500);
  key(Key::Bottom);
  key(Key::Top);
  key(Key::Top);
  shot("31_test_bledy");
  key(Key::Home);
  check(!selftest::active(), "test sprzętu zakończony");
  printf("asercje: %s\n", failures ? "BLEDY" : "wszystkie OK");
  return failures ? 1 : 0;
}
