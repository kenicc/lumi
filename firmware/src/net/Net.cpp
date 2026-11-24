#include "net/Net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>

#include "app/Events.h"
#include "audio/AudioOut.h"

namespace net {

namespace {

SemaphoreHandle_t mtx;
Settings cfgNet;
bool cfgDirty = false;
wx::Forecast fc;
volatile bool weatherReq = true;
char devId[24];

WiFiMulti* multi = nullptr;
bool wifiOff = true;
bool ntpStarted = false;
uint32_t lastWeather = 0, lastSync = 0;

constexpr uint32_t WEATHER_EVERY_MS = 30UL * 60 * 1000;
constexpr const char* TZ_PL = "CET-1CEST,M3.5.0,M10.5.0/3";

Settings snapshot() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  Settings s = cfgNet;
  xSemaphoreGive(mtx);
  return s;
}

void rebuildWifi(const Settings& s) {
  delete multi;
  multi = new WiFiMulti();
  for (const auto& w : s.wifi) multi->addAP(w.ssid.c_str(), w.pass.c_str());
}

void fetchWeather(const Settings& s) {
  char url[256];
  snprintf(url, sizeof url,
           "http://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
           "&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&timezone=Europe%%2FWarsaw&forecast_days=2",
           s.lat, s.lon);
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(url)) return;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[pogoda] HTTP %d\n", code);
    http.end();
    return;
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  if (e) {
    Serial.printf("[pogoda] JSON %s\n", e.c_str());
    return;
  }
  wx::Forecast f;
  f.tempNow = doc["current"]["temperature_2m"] | 0.0f;
  f.codeNow = doc["current"]["weather_code"] | 0;
  JsonObject d = doc["daily"];
  for (int i = 0; i < 2; i++) {
    wx::Day& day = i == 0 ? f.today : f.tomorrow;
    day.code = d["weather_code"][i] | 0;
    day.tMax = d["temperature_2m_max"][i] | 0.0f;
    day.tMin = d["temperature_2m_min"][i] | 0.0f;
    day.valid = !d["weather_code"][i].isNull();
  }
  f.valid = !doc["current"].isNull();
  f.fetchedAt = millis();
  xSemaphoreTake(mtx, portMAX_DELAY);
  fc = f;
  xSemaphoreGive(mtx);
  bus::post(Event(Ev::WeatherUpdated));
}

// Pobiera dokument konfiguracji. Zapisuje go tylko, gdy ma wyższą wersję niż obecna.
void syncConfig(const Settings& s) {
  bool https = s.syncUrl.rfind("https://", 0) == 0;
  WiFiClientSecure secure;
  WiFiClient plain;
  HTTPClient http;
  http.setTimeout(8000);
  if (https) {
    File ca = LittleFS.open("/sync_ca.pem", "r");
    if (!ca) {
      Serial.println("[sync] brak /sync_ca.pem - bez certyfikatu nie pobieram konfiguracji");
      return;
    }
    String pem = ca.readString();
    ca.close();
    static String keep;  // WiFiClientSecure trzyma wskaźnik
    keep = pem;
    secure.setCACert(keep.c_str());
    if (!http.begin(secure, s.syncUrl.c_str())) return;
  } else {
#ifndef LUMI_ALLOW_HTTP_SYNC
    Serial.println("[sync] tylko HTTPS (LUMI_ALLOW_HTTP_SYNC pozwala na HTTP w sieci domowej do testow)");
    return;
#else
    if (!http.begin(plain, s.syncUrl.c_str())) return;
#endif
  }
  http.addHeader("X-Lumi-Device", devId);
  http.addHeader("X-Lumi-Version", String(s.version));
  int code = http.GET();
  if (code != 200) {
    if (code != 304) Serial.printf("[sync] HTTP %d\n", code);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    Serial.println("[sync] nieczytelny dokument, zostaje poprzednia konfiguracja");
    return;
  }
  int v = doc["version"] | 0;
  if (v <= s.version) return;
  if (saveConfigFile(body.c_str(), body.length())) {
    Serial.printf("[sync] nowa konfiguracja v%d\n", v);
    bus::post(Event(Ev::ConfigUpdated, v));
  }
}

void task(void*) {
  WiFi.mode(WIFI_OFF);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Settings s = snapshot();
    xSemaphoreTake(mtx, portMAX_DELAY);
    bool dirty = cfgDirty;
    cfgDirty = false;
    xSemaphoreGive(mtx);
    if (dirty) rebuildWifi(s);

    // Cisza elektryczna: radio FM gra, a nic nie potrzebuje sieci
    bool quiet = audio::fmActive();
    if (quiet || s.wifi.empty()) {
      if (!wifiOff) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        wifiOff = true;
      }
      continue;
    }
    if (wifiOff) {
      WiFi.mode(WIFI_STA);
      wifiOff = false;
    }
    if (!multi) rebuildWifi(s);
    if (multi->run(5000) != WL_CONNECTED) continue;

    if (!ntpStarted) {
      configTzTime(TZ_PL, "pool.ntp.org", "time.google.com");
      ntpStarted = true;
    }
    uint32_t now = millis();
    if (s.weather && (weatherReq || !lastWeather || now - lastWeather > WEATHER_EVERY_MS)) {
      weatherReq = false;
      lastWeather = now;
      fetchWeather(s);
    }
    if (!s.syncUrl.empty() && (!lastSync || now - lastSync > s.syncIntervalSec * 1000UL)) {
      lastSync = now;
      syncConfig(s);
    }
  }
}

}  // namespace

void begin(const char* deviceId) {
  strlcpy(devId, deviceId, sizeof devId);
  mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(task, "net", 8192, nullptr, 2, nullptr, 0);
}

void apply(const Settings& s) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool coordsChanged = s.lat != cfgNet.lat || s.lon != cfgNet.lon;
  cfgNet = s;
  cfgDirty = true;
  xSemaphoreGive(mtx);
  if (coordsChanged) weatherReq = true;
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

wx::Forecast forecast() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  wx::Forecast f = fc;
  xSemaphoreGive(mtx);
  return f;
}

void refreshWeather() { weatherReq = true; }

bool saveConfigFile(const char* json, size_t len) {
  File f = LittleFS.open("/config.tmp", "w");
  if (!f) return false;
  size_t w = f.write(reinterpret_cast<const uint8_t*>(json), len);
  f.close();
  if (w != len) return false;
  LittleFS.remove("/config.bak");
  if (LittleFS.exists("/config.json")) LittleFS.rename("/config.json", "/config.bak");
  return LittleFS.rename("/config.tmp", "/config.json");
}

}  // namespace net
