// Testy logiki niezależnej od sprzętu: pio test -e native
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include <string>

#include "core/AtParse.h"
#include "core/ButtonFilter.h"
#include "core/Calendar.h"
#include "core/Config.h"
#include "core/Pairing.h"
#include "core/Weather.h"

void setUp() {}
void tearDown() {}

// --- przyciski ---

static BtnEvent run(ButtonFilter& f, bool raw, uint32_t from, uint32_t to, BtnEvent want) {
  BtnEvent got = BtnEvent::None;
  for (uint32_t t = from; t <= to; t += 5) {
    BtnEvent e = f.update(raw, t);
    if (e != BtnEvent::None && got == BtnEvent::None) got = e;
  }
  (void)want;
  return got;
}

void test_button_short_after_debounce() {
  ButtonFilter f;
  TEST_ASSERT_EQUAL(BtnEvent::Short, run(f, true, 0, 100, BtnEvent::Short));
  TEST_ASSERT_EQUAL(BtnEvent::None, run(f, false, 105, 300, BtnEvent::None));
}

void test_button_bounce_ignored() {
  ButtonFilter f;
  // 10 ms impulsy krótsze niż debounce
  BtnEvent got = BtnEvent::None;
  for (uint32_t t = 0; t < 200; t += 5) {
    BtnEvent e = f.update((t / 10) % 2 == 0, t);
    if (e != BtnEvent::None) got = e;
  }
  TEST_ASSERT_EQUAL(BtnEvent::None, got);
}

void test_button_tremor_min_press() {
  ButtonFilter f;
  ButtonTiming t;
  t.minPressMs = 300;
  f.setTiming(t);
  // 150 ms dotknięcie - za krótko
  TEST_ASSERT_EQUAL(BtnEvent::None, run(f, true, 0, 150, BtnEvent::None));
  TEST_ASSERT_EQUAL(BtnEvent::None, run(f, false, 155, 400, BtnEvent::None));
  // 500 ms - przyjęte
  TEST_ASSERT_EQUAL(BtnEvent::Short, run(f, true, 405, 905, BtnEvent::Short));
}

void test_button_repeat_ignored() {
  ButtonFilter f;
  ButtonTiming t;
  t.repeatIgnoreMs = 600;
  f.setTiming(t);
  TEST_ASSERT_EQUAL(BtnEvent::Short, run(f, true, 0, 100, BtnEvent::Short));
  run(f, false, 105, 200, BtnEvent::None);
  // drgnięcie dłoni 200 ms po pierwszym naciśnięciu
  TEST_ASSERT_EQUAL(BtnEvent::None, run(f, true, 205, 300, BtnEvent::None));
  run(f, false, 305, 900, BtnEvent::None);
  TEST_ASSERT_EQUAL(BtnEvent::Short, run(f, true, 905, 1000, BtnEvent::Short));
}

void test_button_long_press() {
  ButtonFilter f;
  ButtonTiming t;
  t.longMs = 3000;
  f.setTiming(t);
  TEST_ASSERT_EQUAL(BtnEvent::HoldStart, run(f, true, 0, 100, BtnEvent::HoldStart));
  TEST_ASSERT_EQUAL(BtnEvent::Long, run(f, true, 105, 3100, BtnEvent::Long));
  TEST_ASSERT_EQUAL(BtnEvent::None, run(f, false, 3105, 3300, BtnEvent::None));
}

void test_button_long_released_early_is_short() {
  ButtonFilter f;
  ButtonTiming t;
  t.longMs = 3000;
  f.setTiming(t);
  run(f, true, 0, 500, BtnEvent::HoldStart);
  TEST_ASSERT_EQUAL(BtnEvent::Short, run(f, false, 505, 700, BtnEvent::Short));
}

// --- modem ---

void test_clip() {
  char n[24];
  TEST_ASSERT_TRUE(at::parseClip("+CLIP: \"+48600100200\",145,\"\",0,\"\",0", n, sizeof n));
  TEST_ASSERT_EQUAL_STRING("+48600100200", n);
  TEST_ASSERT_TRUE(at::parseClip("+CLIP: \"\",128", n, sizeof n));
  TEST_ASSERT_EQUAL_STRING("", n);
  TEST_ASSERT_FALSE(at::parseClip("RING", n, sizeof n));
}

void test_clcc() {
  at::Clcc c;
  TEST_ASSERT_TRUE(at::parseClcc("+CLCC: 1,1,4,0,0,\"+48600100200\",145", c));
  TEST_ASSERT_EQUAL(1, c.dir);
  TEST_ASSERT_EQUAL(4, c.stat);
  TEST_ASSERT_EQUAL_STRING("+48600100200", c.number);
  TEST_ASSERT_TRUE(at::parseClcc("+CLCC: 1,0,0,0,0", c));
  TEST_ASSERT_EQUAL(0, c.stat);
}

void test_csq_reg_cops() {
  int r, st, act;
  char op[24];
  TEST_ASSERT_TRUE(at::parseCsq("+CSQ: 20,99", r));
  TEST_ASSERT_EQUAL(4, at::csqToBars(r));
  TEST_ASSERT_EQUAL(0, at::csqToBars(99));
  TEST_ASSERT_TRUE(at::parseRegStat("+CEREG: 0,5", st));
  TEST_ASSERT_TRUE(at::isRegistered(st));
  TEST_ASSERT_TRUE(at::parseCops("+COPS: 0,0,\"Plus\",7", op, sizeof op, act));
  TEST_ASSERT_EQUAL_STRING("Plus", op);
  TEST_ASSERT_EQUAL(7, act);
  TEST_ASSERT_TRUE(at::parseCops("+COPS: 0", op, sizeof op, act));
  TEST_ASSERT_EQUAL(-1, act);
}

void test_cclk() {
  int64_t t;
  // 24.11.2026 22:31:05 czasu polskiego (UTC+1 = +04 kwadranse) -> 21:31:05 UTC
  TEST_ASSERT_TRUE(at::parseCclk("+CCLK: \"26/11/24,22:31:05+04\"", t));
  TEST_ASSERT_EQUAL_INT64(1795555865LL, t);
  TEST_ASSERT_FALSE(at::parseCclk("+CCLK: \"70/01/01,00:00:05+00\"", t));
}

void test_numbers() {
  TEST_ASSERT_TRUE(at::sameNumber("+48 600 100 200", "600100200"));
  TEST_ASSERT_TRUE(at::sameNumber("0048600100200", "+48600100200"));
  TEST_ASSERT_FALSE(at::sameNumber("600100201", "600100200"));
  char out[24];
  at::formatNumber("+48600100200", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("600 100 200", out);
  at::formatNumber("124", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("124", out);
}

void test_cbc() {
  int mv;
  TEST_ASSERT_TRUE(at::parseCbc("+CBC: 0,76,3987", mv));
  TEST_ASSERT_EQUAL(3987, mv);
  TEST_ASSERT_FALSE(at::parseCbc("+CBC: 0,0,0", mv));
}

void test_pairing() {
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:LUMI-A1B2C3;P:12345678;;", pairing::wifiQr("LUMI-A1B2C3", "12345678").c_str());
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:Dom\\;1\\:a;P:x\\\"y;;", pairing::wifiQr("Dom;1:a", "x\"y").c_str());
  TEST_ASSERT_EQUAL_STRING("https://lumi.app/pair?d=LUMI-1&c=ABCD", pairing::pairUrl("https://lumi.app/", "LUMI-1", "ABCD").c_str());
}

// --- kalendarz i pogoda ---

void test_calendar() {
  char b[48];
  cal::dayLine(2, 19, true, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("Wtorek wieczorem", b);
  cal::dateLine(24, 11, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("24 listopada", b);
  TEST_ASSERT_EQUAL_STRING("w nocy", cal::partOfDay(2));
}

void test_weather() {
  TEST_ASSERT_EQUAL_STRING("pochmurno", wx::textFor(3));
  TEST_ASSERT_EQUAL_STRING("Weź parasol", wx::hintFor(61, 8));
  TEST_ASSERT_EQUAL_STRING("Uwaga, ślisko", wx::hintFor(61, 0));
  TEST_ASSERT_EQUAL_STRING("", wx::hintFor(0, 10));
}

// --- konfiguracja ---

static const char* kCfg = R"({
  "version": 7, "changedBy": "Ania", "changeText": "dodała kontakt Basia",
  "contacts": [
    {"id":"a","name":"Ania","number":"+48600100200","color":"#3D8BFD","autoAnswer":true},
    {"id":"b","name":"Basia","number":"601 200 300"},
    {"name":"","number":"1"}
  ],
  "calls": {"onlyContacts": true},
  "home": {"slots": ["contact:a", "radio", "weather"]},
  "media": [
    {"type":"fm","name":"Jedynka","freq":89.8},
    {"type":"fm","name":"zła","freq":150},
    {"type":"web","name":"Classic","url":"http://example.com/s.mp3"},
    {"type":"playlist","name":"Od Ani","from":"Ania","urls":["http://example.com/1.mp3"]}
  ],
  "sound": {"quiet": {"from": "22:00", "to": "07:00"}, "maxVolume": 8, "startVolume": 9},
  "access": {"locked": ["music"], "minPressMs": 5000}
})";

void test_config_parse() {
  cfg::Config c;
  std::string err;
  TEST_ASSERT_TRUE_MESSAGE(cfg::parse(kCfg, strlen(kCfg), c, err), err.c_str());
  TEST_ASSERT_EQUAL(7, c.version);
  TEST_ASSERT_EQUAL(2, (int)c.contacts.size());
  TEST_ASSERT_EQUAL_HEX32(0x3D8BFD, c.contacts[0].color);
  TEST_ASSERT_EQUAL(1, c.contactIndexByNumber("+48601200300"));
  TEST_ASSERT_EQUAL(-1, c.contactIndexByNumber("500000000"));
  TEST_ASSERT_TRUE(c.onlyContacts);
  TEST_ASSERT_EQUAL(cfg::SlotKind::Contact, c.home[0].kind);
  TEST_ASSERT_EQUAL(cfg::SlotKind::Media, c.home[1].kind);
  TEST_ASSERT_EQUAL(cfg::SlotKind::Weather, c.home[2].kind);
  TEST_ASSERT_EQUAL(3, (int)c.mediaList.size());
  TEST_ASSERT_TRUE(c.inQuietHours(23 * 60));
  TEST_ASSERT_TRUE(c.inQuietHours(6 * 60));
  TEST_ASSERT_FALSE(c.inQuietHours(12 * 60));
  TEST_ASSERT_EQUAL(8, c.maxVolume);
  TEST_ASSERT_EQUAL(8, c.startVolume);  // przycięte do maxVolume
  TEST_ASSERT_EQUAL(1500, c.minPressMs);
  TEST_ASSERT_TRUE(c.lockMusic);
  TEST_ASSERT_FALSE(c.lockCall);
}

void test_config_broken() {
  cfg::Config c;
  std::string err;
  TEST_ASSERT_FALSE(cfg::parse("{\"version\": ", 12, c, err));
  TEST_ASSERT_FALSE(err.empty());
}

// Kontrakt z aplikacją rodziny: dokument wygenerowany przez app/src/lib/config.ts
void test_contract() {
  FILE* f = fopen("../shared/config-contract.json", "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, "brak shared/config-contract.json");
  std::string s;
  char b[1024];
  size_t n;
  while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
  fclose(f);
  cfg::Config c;
  std::string err;
  TEST_ASSERT_TRUE_MESSAGE(cfg::parse(s.data(), s.size(), c, err), err.c_str());
  TEST_ASSERT_EQUAL(7, c.version);
  TEST_ASSERT_EQUAL_STRING("Ania", c.changedBy.c_str());
  TEST_ASSERT_EQUAL(2, (int)c.contacts.size());
  TEST_ASSERT_TRUE(c.contacts[0].autoAnswer);
  TEST_ASSERT_EQUAL(0, c.contactIndexByNumber("600 100 200"));
  TEST_ASSERT_EQUAL(cfg::SlotKind::Contact, c.home[0].kind);
  TEST_ASSERT_EQUAL(3, (int)c.mediaList.size());
  TEST_ASSERT_EQUAL(cfg::MediaType::Playlist, c.mediaList[2].type);
  TEST_ASSERT_EQUAL_STRING("Ania", c.mediaList[2].from.c_str());
  TEST_ASSERT_TRUE(c.lockMusic);
  TEST_ASSERT_TRUE(c.inQuietHours(23 * 60));
  TEST_ASSERT_EQUAL(1, (int)c.wifi.size());
  TEST_ASSERT_EQUAL(20, c.syncIntervalSec);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_button_short_after_debounce);
  RUN_TEST(test_button_bounce_ignored);
  RUN_TEST(test_button_tremor_min_press);
  RUN_TEST(test_button_repeat_ignored);
  RUN_TEST(test_button_long_press);
  RUN_TEST(test_button_long_released_early_is_short);
  RUN_TEST(test_clip);
  RUN_TEST(test_clcc);
  RUN_TEST(test_csq_reg_cops);
  RUN_TEST(test_cclk);
  RUN_TEST(test_numbers);
  RUN_TEST(test_cbc);
  RUN_TEST(test_pairing);
  RUN_TEST(test_calendar);
  RUN_TEST(test_weather);
  RUN_TEST(test_config_parse);
  RUN_TEST(test_config_broken);
  RUN_TEST(test_contract);
  return UNITY_END();
}
