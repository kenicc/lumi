#include "../BuildConfig.h"
#include "Identity.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_random.h>

#ifndef LUMI_SERVER
#define LUMI_SERVER ""
#endif

namespace identity {

namespace {
Preferences p;
char devId[16], sec[65], pair[9], apPass[9];
SemaphoreHandle_t mtx;

void randomHex(char* out, size_t bytes) {
  for (size_t i = 0; i < bytes; i++) snprintf(out + i * 2, 3, "%02x", (unsigned)(esp_random() & 0xFF));
}

void randomCode(char* out, size_t n, const char* alphabet) {
  size_t a = strlen(alphabet);
  for (size_t i = 0; i < n; i++) out[i] = alphabet[esp_random() % a];
  out[n] = 0;
}

void generate() {
  randomHex(sec, 32);
  randomCode(pair, 8, "ABCDEFGHJKMNPQRSTUVWXYZ23456789");  // bez I/L/O/0/1 - łatwo przepisać
  randomCode(apPass, 8, "0123456789");
  p.putString("sec", sec);
  p.putString("pair", pair);
  p.putString("ap", apPass);
}
}  // namespace

void begin() {
  mtx = xSemaphoreCreateMutex();
  uint64_t mac = ESP.getEfuseMac();
  snprintf(devId, sizeof devId, "LUMI-%06X", (unsigned)((mac >> 24) & 0xFFFFFF));
  p.begin("lumi-id", false);
  if (!p.isKey("sec")) {
    generate();
  } else {
    strlcpy(sec, p.getString("sec").c_str(), sizeof sec);
    strlcpy(pair, p.getString("pair").c_str(), sizeof pair);
    strlcpy(apPass, p.getString("ap").c_str(), sizeof apPass);
  }
}

const char* id() { return devId; }
const char* secret() { return sec; }
const char* pairCode() { return pair; }
const char* apPassword() { return apPass; }

// Adres serwera trzymany w RAM: zadanie sieci pyta o niego co sekundę, a odczyt z NVS to flash i mutex
std::string srvCache;
bool srvLoaded = false;

std::string server() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  if (!srvLoaded) {
    String s = p.isKey("srv") ? p.getString("srv", LUMI_SERVER) : String(LUMI_SERVER);
    srvCache = s.c_str();
    while (!srvCache.empty() && srvCache.back() == '/') srvCache.pop_back();
    srvLoaded = true;
  }
  std::string r = srvCache;
  xSemaphoreGive(mtx);
  return r;
}
void setServer(const std::string& url) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  p.putString("srv", url.c_str());
  srvLoaded = false;  // następny odczyt weźmie nową wartość
  xSemaphoreGive(mtx);
}

// Lista sieci w RAM: zadanie sieci pyta o nią co sekundę (wcześniej odczyt NVS i parsowanie JSON za każdym razem)
std::vector<cfg::Wifi> wifiCache;
bool wifiLoaded = false;

std::vector<cfg::Wifi> loadWifiLocked() {
  if (!wifiLoaded) {
    wifiCache.clear();
    if (p.isKey("wifi")) {
      JsonDocument d;
      if (!deserializeJson(d, p.getString("wifi", "[]")))
        for (JsonObject o : d.as<JsonArray>()) wifiCache.push_back({o["s"] | "", o["p"] | ""});
    }
    wifiLoaded = true;
  }
  return wifiCache;
}

std::vector<cfg::Wifi> wifi() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  std::vector<cfg::Wifi> out = loadWifiLocked();
  xSemaphoreGive(mtx);
  return out;
}

void addWifi(const std::string& ssid, const std::string& pass) {
  xSemaphoreTake(mtx, portMAX_DELAY);  // całość pod blokadą: dwa równoczesne dopisania nie zgubią sieci
  auto list = loadWifiLocked();
  for (auto it = list.begin(); it != list.end();)
    it = (it->ssid == ssid) ? list.erase(it) : it + 1;
  list.insert(list.begin(), {ssid, pass});
  if (list.size() > 5) list.resize(5);
  JsonDocument d;
  for (auto& w : list) {
    JsonObject o = d.add<JsonObject>();
    o["s"] = w.ssid;
    o["p"] = w.pass;
  }
  String s;
  serializeJson(d, s);
  p.putString("wifi", s);
  wifiCache = list;
  xSemaphoreGive(mtx);
}

void factoryReset() {
  p.clear();
  srvLoaded = false;
  wifiLoaded = false;
  generate();
  LittleFS.remove("/config.json");
  LittleFS.remove("/config.bak");
  Preferences app;
  app.begin("lumi", false);
  app.clear();
  app.end();
}

}  // namespace identity
