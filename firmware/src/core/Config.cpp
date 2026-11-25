#include "core/Config.h"

#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>

#include "core/AtParse.h"

namespace cfg {

namespace {

template <typename T>
T clampv(T v, T lo, T hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

int parseHm(const char* s) {
  if (!s) return -1;
  int h, m;
  if (sscanf(s, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

std::string str(JsonVariantConst v, const char* def = "") {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string(def);
}

void parseSlot(const char* s, HomeSlot& slot) {
  slot = HomeSlot{};
  if (!s) return;
  if (strncmp(s, "contact:", 8) == 0) {
    slot.kind = SlotKind::Contact;
    slot.contactId = s + 8;
  } else if (!strcmp(s, "media") || !strcmp(s, "radio") || !strcmp(s, "music")) {
    slot.kind = SlotKind::Media;
  } else if (!strcmp(s, "weather")) {
    slot.kind = SlotKind::Weather;
  }
}

}  // namespace

uint32_t parseColor(const char* hex, uint32_t fallback) {
  if (!hex || hex[0] != '#' || strlen(hex) != 7) return fallback;
  char* end = nullptr;
  unsigned long v = strtoul(hex + 1, &end, 16);
  return (end && *end == 0) ? (uint32_t)v : fallback;
}

const Contact* Config::contactById(const std::string& id) const {
  for (const auto& c : contacts)
    if (c.id == id) return &c;
  return nullptr;
}

int Config::contactIndexByNumber(const char* number) const {
  if (!number || !*number) return -1;
  for (size_t i = 0; i < contacts.size(); i++)
    if (at::sameNumber(contacts[i].number.c_str(), number)) return (int)i;
  return -1;
}

bool reminderDue(const Reminder& r, int wday, int minuteOfDay) {
  int bit = (wday + 6) % 7;  // tm: 0 = niedziela -> bit 6
  return r.minute == minuteOfDay && (r.days & (1 << bit));
}

bool Config::inQuietHours(int m) const {
  if (quietFrom < 0 || quietTo < 0 || quietFrom == quietTo) return false;
  if (quietFrom < quietTo) return m >= quietFrom && m < quietTo;
  return m >= quietFrom || m < quietTo;  // przez północ, np. 22:00-07:00
}

bool parse(const char* json, size_t len, Config& out, std::string& err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json, len);
  if (e) {
    err = std::string("JSON: ") + e.c_str();
    return false;
  }
  if (!doc.is<JsonObjectConst>()) {
    err = "JSON: oczekiwany obiekt";
    return false;
  }
  Config c;
  c.version = doc["version"] | 0;
  c.changedBy = str(doc["changedBy"]);
  c.changeText = str(doc["changeText"]);

  JsonObjectConst f = doc["features"];
  c.calls = f["calls"] | true;
  c.media = f["media"] | true;
  c.weather = f["weather"] | true;

  for (JsonObjectConst o : doc["contacts"].as<JsonArrayConst>()) {
    Contact k;
    k.id = str(o["id"]);
    k.name = str(o["name"]);
    k.number = str(o["number"]);
    k.color = parseColor(o["color"] | "", k.color);
    k.autoAnswer = o["autoAnswer"] | false;
    if (k.name.empty() || k.number.empty()) continue;  // niepełny kontakt pomijamy, nie przerywamy
    if (k.id.empty()) k.id = k.name;
    c.contacts.push_back(k);
  }
  c.onlyContacts = doc["calls"]["onlyContacts"] | false;

  JsonObjectConst home = doc["home"];
  JsonArrayConst slots = home["slots"];
  for (int i = 0; i < 3; i++) parseSlot(slots[i].as<const char*>(), c.home[i]);
  c.partOfDay = home["partOfDay"] | true;

  for (JsonObjectConst o : doc["media"].as<JsonArrayConst>()) {
    Media m;
    const char* t = o["type"] | "fm";
    m.name = str(o["name"]);
    m.from = str(o["from"]);
    if (!strcmp(t, "fm")) {
      m.type = MediaType::Fm;
      m.freq = o["freq"] | 0.0f;
      if (m.freq < 76.0f || m.freq > 108.0f) continue;
    } else if (!strcmp(t, "web")) {
      m.type = MediaType::Web;
      m.urls.push_back(str(o["url"]));
      if (m.urls[0].empty()) continue;
    } else if (!strcmp(t, "playlist")) {
      m.type = MediaType::Playlist;
      for (JsonVariantConst u : o["urls"].as<JsonArrayConst>()) m.urls.push_back(str(u));
      if (m.urls.empty()) continue;
    } else {
      continue;
    }
    if (m.name.empty()) continue;
    c.mediaList.push_back(m);
  }

  JsonObjectConst w = doc["weather"];
  c.place = str(w["place"]);
  c.lat = w["lat"] | c.lat;
  c.lon = w["lon"] | c.lon;

  JsonObjectConst d = doc["display"];
  c.textSize = clampv<int>(d["textSize"] | 0, 0, 2);
  c.highContrast = d["highContrast"] | false;
  c.brightness = clampv<int>(d["brightness"] | 80, 10, 100);
  c.screenOffSec = clampv<int>(d["screenOffSec"] | 60, 10, 600);

  JsonObjectConst s = doc["sound"];
  c.maxVolume = clampv<int>(s["maxVolume"] | 10, 1, 10);
  c.ringVolume = clampv<int>(s["ringVolume"] | 8, 1, 10);
  c.startVolume = clampv<int>(s["startVolume"] | 5, 1, (int)c.maxVolume);
  c.keyClick = s["keyClick"] | true;
  c.quietFrom = parseHm(s["quiet"]["from"] | (const char*)nullptr);
  c.quietTo = parseHm(s["quiet"]["to"] | (const char*)nullptr);

  JsonObjectConst a = doc["access"];
  c.autoAnswerSec = clampv<int>(a["autoAnswerSec"] | 5, 3, 10);
  c.oneButton = a["oneButton"] | false;
  c.minPressMs = clampv<int>(a["minPressMs"] | 0, 0, 1500);
  c.repeatIgnoreMs = clampv<int>(a["repeatIgnoreMs"] | 250, 0, 2000);
  c.allowPowerOff = a["allowPowerOff"] | true;
  for (JsonVariantConst l : a["locked"].as<JsonArrayConst>()) {
    const char* b = l.as<const char*>();
    if (!b) continue;
    if (!strcmp(b, "music")) c.lockMusic = true;
    if (!strcmp(b, "call")) c.lockCall = true;
  }

  for (JsonObjectConst o : doc["wifi"].as<JsonArrayConst>()) {
    Wifi wf{str(o["ssid"]), str(o["pass"])};
    if (!wf.ssid.empty()) c.wifi.push_back(wf);
  }
  c.syncIntervalSec = clampv<int>(doc["sync"]["intervalSec"] | 20, 10, 3600);

  for (JsonObjectConst o : doc["reminders"].as<JsonArrayConst>()) {
    Reminder r;
    r.id = str(o["id"]);
    r.text = str(o["text"]);
    r.minute = parseHm(o["time"] | (const char*)nullptr);
    r.confirm = o["confirm"] | true;
    uint8_t mask = 0;
    for (JsonVariantConst d : o["days"].as<JsonArrayConst>()) {
      int v = d | 0;
      if (v >= 1 && v <= 7) mask |= 1 << (v - 1);
    }
    r.days = mask ? mask : 0x7F;  // brak dni = codziennie
    if (r.id.empty() || r.text.empty() || r.minute < 0) continue;
    if (c.reminders.size() < 20) c.reminders.push_back(r);
  }

  out = std::move(c);
  return true;
}

}  // namespace cfg
