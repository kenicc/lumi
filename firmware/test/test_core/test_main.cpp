// Testy logiki niezależnej od sprzętu: pio test -e native
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include <string>

#include "core/AtParse.h"
#include "core/ButtonFilter.h"
#include "core/Calendar.h"
#include "core/Config.h"
#include "core/LocalEdit.h"
#include "core/Pairing.h"
#include "core/ReminderQueue.h"
#include "core/RtcCodec.h"
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
  TEST_ASSERT_TRUE(at::sameNumber("48600100200", "600 100 200"));
  TEST_ASSERT_TRUE(at::sameNumber("124", "124"));  // numer skrócony / serwisowy
  // obcy numer z tą samą końcówką to nie ten kontakt
  TEST_ASSERT_FALSE(at::sameNumber("+491701234567", "701234567"));
  TEST_ASSERT_FALSE(at::sameNumber("+48701234567", "+49701234567"));
  TEST_ASSERT_FALSE(at::sameNumber("", "600100200"));
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

void test_reminders() {
  const char* j = R"({"reminders":[
    {"id":"leki","text":"Weź leki","time":"08:00","days":[1,2,3,4,5]},
    {"id":"woda","text":"Napij się wody","time":"14:30","confirm":false},
    {"id":"zly","text":"x","time":"25:00"}]})";
  cfg::Config c;
  std::string err;
  TEST_ASSERT_TRUE(cfg::parse(j, strlen(j), c, err));
  TEST_ASSERT_EQUAL(2, (int)c.reminders.size());
  const cfg::Reminder& leki = c.reminders[0];
  TEST_ASSERT_TRUE(cfg::reminderDue(leki, 2, 8 * 60));   // wtorek 8:00
  TEST_ASSERT_FALSE(cfg::reminderDue(leki, 0, 8 * 60));  // niedziela
  TEST_ASSERT_FALSE(cfg::reminderDue(leki, 2, 8 * 60 + 1));
  TEST_ASSERT_TRUE(cfg::reminderDue(c.reminders[1], 0, 14 * 60 + 30));  // codziennie
  TEST_ASSERT_FALSE(c.reminders[1].confirm);
}

void test_reminder_queue() {
  rem::Queue q;
  q.add({"a", "Leki", 480, true, 0, 0});
  q.add({"b", "Obiad", 480, false, 0, 0});
  TEST_ASSERT_TRUE(q.start(1000));
  TEST_ASSERT_EQUAL_STRING("a", q.current().id.c_str());
  TEST_ASSERT_FALSE(q.start(1000));  // jedno naraz

  // odłożenie: wraca po 10 min, najwyżej 2 razy
  TEST_ASSERT_TRUE(q.snooze(1000));
  TEST_ASSERT_TRUE(q.start(2000));  // w międzyczasie drugie przypomnienie
  TEST_ASSERT_EQUAL_STRING("b", q.current().id.c_str());
  q.finish();
  TEST_ASSERT_FALSE(q.start(2000 + 60000));  // "a" jeszcze odłożone
  uint32_t t = 1000 + rem::Queue::SNOOZE_MS;
  TEST_ASSERT_TRUE(q.start(t));
  TEST_ASSERT_TRUE(q.snooze(t));
  t += rem::Queue::SNOOZE_MS;
  TEST_ASSERT_TRUE(q.start(t));
  TEST_ASSERT_FALSE(q.canSnooze());
  TEST_ASSERT_FALSE(q.snooze(t));

  // dźwięk co minutę, rozmowa wstrzymuje licznik, po 10 min bez potwierdzenia
  TEST_ASSERT_FALSE(q.tick(t + 30000, false, 100).beep);
  TEST_ASSERT_TRUE(q.tick(t + 60000, false, 100).beep);
  q.tick(t + 9 * 60000, true, 5 * 60000);  // 5 min rozmowy
  TEST_ASSERT_EQUAL((int)rem::Timeout::None, (int)q.tick(t + 11 * 60000, false, 100).timeout);
  TEST_ASSERT_EQUAL((int)rem::Timeout::Missed, (int)q.tick(t + 16 * 60000, false, 100).timeout);
  TEST_ASSERT_FALSE(q.active());

  // bez potwierdzania: po czasie "przypomniane", nie "bez potwierdzenia"
  rem::Queue q2;
  q2.add({"c", "Woda", 600, false, 0, 0});
  q2.start(0);
  TEST_ASSERT_EQUAL((int)rem::Timeout::Shown, (int)q2.tick(rem::Queue::TIMEOUT_MS + 1, false, 100).timeout);
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
  TEST_ASSERT_EQUAL_STRING("0123456789abcdef", c.contacts[0].photo.c_str());
  TEST_ASSERT_TRUE(c.contacts[1].photo.empty());
  TEST_ASSERT_EQUAL(0, c.contactIndexByNumber("600 100 200"));
  TEST_ASSERT_EQUAL(cfg::SlotKind::Contact, c.home[0].kind);
  TEST_ASSERT_EQUAL(4, (int)c.mediaList.size());
  TEST_ASSERT_EQUAL(cfg::MediaType::Playlist, c.mediaList[2].type);
  TEST_ASSERT_EQUAL_STRING("Ania", c.mediaList[2].from.c_str());
  // podcast: lista odcinków od serwera, bez adresu kanału
  TEST_ASSERT_EQUAL(cfg::MediaType::Podcast, c.mediaList[3].type);
  TEST_ASSERT_EQUAL(2, (int)c.mediaList[3].urls.size());
  TEST_ASSERT_EQUAL_STRING("Odcinek 2", c.mediaList[3].titles[0].c_str());
  TEST_ASSERT_TRUE(c.lockMusic);
  TEST_ASSERT_TRUE(c.inQuietHours(23 * 60));
  TEST_ASSERT_EQUAL(1, (int)c.wifi.size());
  TEST_ASSERT_EQUAL(20, c.syncIntervalSec);
  TEST_ASSERT_EQUAL(2, (int)c.reminders.size());
  TEST_ASSERT_EQUAL(8 * 60, c.reminders[0].minute);
  TEST_ASSERT_TRUE(cfg::reminderDue(c.reminders[0], 1, 8 * 60));    // poniedziałek
  TEST_ASSERT_FALSE(cfg::reminderDue(c.reminders[0], 6, 8 * 60));   // sobota
  TEST_ASSERT_TRUE(cfg::reminderDue(c.reminders[1], 6, 14 * 60 + 30));
  TEST_ASSERT_FALSE(c.reminders[1].confirm);
}

// --- zegar DS3231 ---

static void test_rtc_codec() {
  // 2026-11-27 13:45:07 UTC
  int64_t t = at::daysFromCivil(2026, 11, 27) * 86400 + 13 * 3600 + 45 * 60 + 7;
  uint8_t r[7];
  rtcc::encode(t, r);
  TEST_ASSERT_EQUAL_HEX8(0x07, r[0]);
  TEST_ASSERT_EQUAL_HEX8(0x45, r[1]);
  TEST_ASSERT_EQUAL_HEX8(0x13, r[2]);
  TEST_ASSERT_EQUAL_HEX8(0x27, r[4]);
  TEST_ASSERT_EQUAL_HEX8(0x11, r[5]);
  TEST_ASSERT_EQUAL_HEX8(0x26, r[6]);
  TEST_ASSERT_EQUAL(6, r[3]);  // piątek (niedziela = 1)
  int64_t back = 0;
  TEST_ASSERT_TRUE(rtcc::decode(r, back));
  TEST_ASSERT_EQUAL_INT64(t, back);
  // przełom roku i rok przestępny
  int64_t leap = at::daysFromCivil(2028, 2, 29) * 86400 + 23 * 3600 + 59 * 60 + 59;
  rtcc::encode(leap, r);
  TEST_ASSERT_TRUE(rtcc::decode(r, back));
  TEST_ASSERT_EQUAL_INT64(leap, back);
  // nowy moduł (2000-01-01) i śmieci są odrzucane
  uint8_t fresh[7] = {0, 0, 0, 1, 1, 1, 0};
  TEST_ASSERT_FALSE(rtcc::decode(fresh, back));
  uint8_t junk[7] = {0x99, 0x99, 0x99, 9, 0x99, 0x99, 0x99};
  TEST_ASSERT_FALSE(rtcc::decode(junk, back));
}

void test_local_edit() {
  using namespace local;
  std::string doc, out, err;
  // nowe urządzenie bez aplikacji: pierwszy kontakt tworzy dokument, który czyta zwykły parser
  Op add;
  add.kind = Kind::AddContact;
  add.id = "d1";
  add.name = "Córka Ania";
  add.number = "+48 600-100-200";
  TEST_ASSERT_TRUE(apply(doc, add, out, err));
  cfg::Config c;
  TEST_ASSERT_TRUE(cfg::parse(out.c_str(), out.size(), c, err));
  TEST_ASSERT_EQUAL(0, c.version);
  TEST_ASSERT_EQUAL(1, (int)c.contacts.size());
  TEST_ASSERT_EQUAL_STRING("+48600100200", c.contacts[0].number.c_str());
  TEST_ASSERT_EQUAL_STRING("Córka Ania", c.contacts[0].name.c_str());
  doc = out;
  // ten sam numer drugi raz, zły numer, za długie imię - odrzucone, dokument bez zmian
  add.id = "d2";
  TEST_ASSERT_FALSE(apply(doc, add, out, err));
  add.number = "12a45";
  TEST_ASSERT_FALSE(apply(doc, add, out, err));
  add.number = "601200300";
  add.name = "Bardzo długie imię ponad limit znaków";
  TEST_ASSERT_FALSE(apply(doc, add, out, err));
  add.name = "Józef";
  TEST_ASSERT_TRUE(apply(doc, add, out, err));
  doc = out;
  // usunięcie czyści też skrót na ekranie głównym
  std::string withSlot = "{\"version\":7,\"contacts\":[{\"id\":\"a\",\"name\":\"A\",\"number\":\"600100200\"}],"
                         "\"home\":{\"slots\":[\"contact:a\",\"media\",\"\"]},\"changedBy\":\"Ania\"}";
  Op rm;
  rm.kind = Kind::RemoveContact;
  rm.id = "a";
  TEST_ASSERT_TRUE(apply(withSlot, rm, out, err));
  TEST_ASSERT_TRUE(cfg::parse(out.c_str(), out.size(), c, err));
  TEST_ASSERT_EQUAL(7, c.version);  // wersja rodziny zostaje
  TEST_ASSERT_EQUAL(0, (int)c.contacts.size());
  TEST_ASSERT_TRUE(c.changedBy.empty());  // to nie zmiana od rodziny
  rm.id = "nie-ma";
  TEST_ASSERT_FALSE(apply(withSlot, rm, out, err));
  // wielkość tekstu i dzwonek w zakresie
  Op ts;
  ts.kind = Kind::TextSize;
  ts.value = 2;
  TEST_ASSERT_TRUE(apply(doc, ts, out, err));
  TEST_ASSERT_TRUE(cfg::parse(out.c_str(), out.size(), c, err));
  TEST_ASSERT_EQUAL(2, c.textSize);
  ts.value = 3;
  TEST_ASSERT_FALSE(apply(doc, ts, out, err));
  Op rv;
  rv.kind = Kind::RingVolume;
  rv.value = 0;
  TEST_ASSERT_FALSE(apply(doc, rv, out, err));
  // zdarzenie dla serwera
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"setting\",\"op\":\"removeContact\",\"id\":\"nie-ma\"}", eventJson(rm).c_str());
  TEST_ASSERT_EQUAL_STRING("", normalizeNumber("+48+600").c_str());
  TEST_ASSERT_EQUAL(5u, utf8Len("Józef"));
}

void test_weather_fresh() {
  wx::Forecast f;
  f.valid = true;
  f.tempNow = 6;
  f.today = {true, 3, 2, 7};
  f.tomorrow = {true, 61, 1, 8};
  f.fetchedAt = 1000;
  f.dayKey = 2026 * 1000 + 328;
  // świeża, ten sam dzień: bez zmian
  wx::Forecast g = wx::fresh(f, 1000 + 60000, f.dayKey);
  TEST_ASSERT_TRUE(g.valid && g.tomorrow.valid);
  // ponad 3 godziny: temperatura "teraz" już nieaktualna
  TEST_ASSERT_FALSE(wx::fresh(f, 1000 + wx::STALE_AFTER_MS + 1, f.dayKey).valid);
  // po północy: wczorajsze jutro to dziś, jutra brak
  g = wx::fresh(f, 1000 + 60000, f.dayKey + 1);
  TEST_ASSERT_TRUE(g.valid);
  TEST_ASSERT_EQUAL(61, g.today.code);
  TEST_ASSERT_FALSE(g.tomorrow.valid);
  // przez Nowy Rok
  f.dayKey = 2026 * 1000 + 364;
  TEST_ASSERT_EQUAL(61, wx::fresh(f, 2000, 2027 * 1000 + 0).today.code);
  // dwa dni później: nic
  TEST_ASSERT_FALSE(wx::fresh(f, 2000, 2027 * 1000 + 1).valid);
  // millis() przekręcone po 49 dniach: różnica bez znaku dalej poprawna
  f.dayKey = -1;
  f.fetchedAt = 0xFFFFFF00u;
  TEST_ASSERT_TRUE(wx::fresh(f, 0x100u, -1).valid);
}

void test_due_window() {
  long from = 0, to = 0;
  // pierwsze sprawdzenie: tylko bieżąca minuta
  TEST_ASSERT_TRUE(rem::dueWindow(-1, 1000, from, to));
  TEST_ASSERT_EQUAL(1000, from);
  TEST_ASSERT_EQUAL(1000, to);
  // ta sama minuta drugi raz: nic
  TEST_ASSERT_FALSE(rem::dueWindow(1000, 1000, from, to));
  // pętla spóźniona o 3 minuty: nadrabiamy wszystkie trzy
  TEST_ASSERT_TRUE(rem::dueWindow(1000, 1003, from, to));
  TEST_ASSERT_EQUAL(1001, from);
  TEST_ASSERT_EQUAL(1003, to);
  // zegar cofnięty o 2 minuty: bez powtórek
  TEST_ASSERT_FALSE(rem::dueWindow(1003, 1001, from, to));
  // duży skok w przód (ustawienie czasu) i w tył (poprawka złego zegara): od bieżącej minuty
  TEST_ASSERT_TRUE(rem::dueWindow(1003, 5000, from, to));
  TEST_ASSERT_EQUAL(5000, from);
  TEST_ASSERT_TRUE(rem::dueWindow(5000, 1000, from, to));
  TEST_ASSERT_EQUAL(1000, from);
  TEST_ASSERT_EQUAL(1000, to);
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
  RUN_TEST(test_reminders);
  RUN_TEST(test_reminder_queue);
  RUN_TEST(test_due_window);
  RUN_TEST(test_weather_fresh);
  RUN_TEST(test_local_edit);
  RUN_TEST(test_contract);
  RUN_TEST(test_rtc_codec);
  return UNITY_END();
}
