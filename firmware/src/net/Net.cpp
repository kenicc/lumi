#include "../BuildConfig.h"
#include "Net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <esp_sntp.h>
#include <atomic>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>

#include <deque>
#include <set>

#include "../Version.h"
#include "../app/Diag.h"
#include "../app/Events.h"
#include "../audio/AudioOut.h"
#include "../hw/Battery.h"
#include "../modem/Modem.h"
#include "../app/App.h"
#include "Identity.h"
#include "Ota.h"
#include "../hw/Rtc.h"

extern "C" const uint8_t lumi_ca_bundle[];

namespace net {

namespace {

SemaphoreHandle_t mtx;
Settings cfgNet;
bool cfgDirty = true;
wx::Forecast fc;
volatile bool weatherReq = true;
volatile bool reportReq = true;
volatile bool provisionReq = false;
volatile bool otaForce = false;
bool msgDue = true;  // wiadomości sprawdzamy w tym samym rytmie co konfigurację
std::deque<std::string> events;  // chroniona przez mtx
constexpr size_t MAX_EVENTS = 30;
ota::Offer offer;
uint32_t lastOtaCheck = 0;
constexpr uint32_t OTA_CHECK_EVERY_MS = 6UL * 3600 * 1000;
volatile Cloud cloudState = Cloud::NoWifi;

WiFiMulti* multi = nullptr;
bool wifiOff = true;
std::atomic<bool> ntpSynced{false};  // ustawiane z zadania SNTP, obsługiwane w zadaniu sieci

// Własna sieć panelu przycisków (prototyp): telefon łączy się wprost z LUMI, więc panel działa także
// w sieciach, które blokują ruch między urządzeniami (szkolne, publiczne, część hotspotów).
std::atomic<bool> panelApOn{false};
bool apUp = false;
constexpr const char* PANEL_SSID = "LUMI-PANEL";
bool ntpStarted = false;
bool helloDone = false;
uint32_t lastWeather = 0, lastSync = 0, lastReport = 0, connectingSince = 0;

constexpr uint32_t WEATHER_EVERY_MS = 30UL * 60 * 1000;
constexpr uint32_t REPORT_EVERY_MS = 5UL * 60 * 1000;
constexpr uint32_t PROVISION_AFTER_MS = 90UL * 1000;  // nieskonfigurowane i nie może się połączyć

// Brak znanej sieci (senior wyszedł z domu, router wyłączony): nie skanuj bez przerwy. Każde skanowanie
// to kilka sekund pracy radia, a radio Wi-Fi to największy pobór prądu po ekranie. Kolejne próby co
// 15 s, 30 s, 1 min... do 5 min, radio wyłączone pomiędzy. Po powrocie do domu połączy się najpóźniej po 5 min.
uint8_t wifiFails = 0;
bool listOnFail = true;  // po zmianie sieci: przy pierwszej porażce pokaż, co LUMI widzi w eterze

// Najczęstsze przyczyny "no matching wifi": sieć 5 GHz (ESP32 widzi tylko 2,4 GHz), literówka
// w nazwie (wielkość liter ma znaczenie), wyłączony hotspot w telefonie.
void listVisibleNetworks(const std::vector<cfg::Wifi>& known) {
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("[wifi] LUMI nie widzi zadnej sieci 2,4 GHz w poblizu");
    return;
  }
  Serial.print("[wifi] szukam:");
  for (const auto& w : known) Serial.printf(" \"%s\"", w.ssid.c_str());
  Serial.printf("\n[wifi] LUMI widzi %d sieci (tylko 2,4 GHz; wielkosc liter ma znaczenie):\n", n);
  for (int i = 0; i < n && i < 15; i++)
    Serial.printf("[wifi]   \"%s\"  sygnal %d dBm\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  WiFi.scanDelete();
}
uint32_t wifiRetryAt = 0;
constexpr uint8_t WIFI_QUICK_TRIES = 3;
constexpr uint32_t WIFI_BACKOFF_MAX_MS = 5UL * 60 * 1000;

uint32_t wifiBackoffMs(uint8_t fails) {
  if (fails <= WIFI_QUICK_TRIES) return 0;
  uint32_t ms = 15000UL << min<uint8_t>(fails - WIFI_QUICK_TRIES - 1, 5);
  return ms > WIFI_BACKOFF_MAX_MS ? WIFI_BACKOFF_MAX_MS : ms;
}
constexpr uint32_t PROVISION_WINDOW_MS = 5UL * 60 * 1000;
constexpr const char* TZ_PL = "CET-1CEST,M3.5.0,M10.5.0/3";

void setCloud(Cloud c) {
  if (cloudState == c) return;
  cloudState = c;
  bus::post(Event(Ev::CloudState, (int32_t)c));
}

Settings snapshot() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  Settings s = cfgNet;
  xSemaphoreGive(mtx);
  return s;
}

std::vector<cfg::Wifi> allNetworks(const Settings& s) {
  std::vector<cfg::Wifi> v = identity::wifi();  // dodane z telefonu mają pierwszeństwo
  for (const auto& w : s.wifi) {
    bool dup = false;
    for (const auto& x : v) dup |= x.ssid == w.ssid;
    if (!dup) v.push_back(w);
  }
  return v;
}

void rebuildWifi(const Settings& s) {
  delete multi;
  multi = new WiFiMulti();
  for (const auto& w : allNetworks(s)) multi->addAP(w.ssid.c_str(), w.pass.c_str());
}

// ---------- konfiguracja Wi-Fi z telefonu (własna sieć LUMI) ----------

WebServer* web = nullptr;
DNSServer* dns = nullptr;
uint32_t provisionUntil = 0;
bool provisionSaved = false;

String htmlEscape(const String& s) {
  String o;
  for (char c : s) {
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

const char* kPageHead =
    "<!doctype html><html lang=pl><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>LUMI</title><style>body{font:18px system-ui;margin:0;padding:20px;background:#0f1113;color:#f5f2ea}"
    "h1{font-size:26px}label{display:block;margin:16px 0 6px}input,select,button{font:inherit;width:100%;"
    "padding:12px;border-radius:10px;border:1px solid #555;box-sizing:border-box;background:#262a30;color:#f5f2ea}"
    "button{background:#1e6b46;border:0;margin-top:22px;font-weight:700}small{color:#bdb8ad}</style>";

void pageRoot() {
  String ssids;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);
  for (int i = 0; i < n && i < 20; i++) {
    String s = htmlEscape(WiFi.SSID(i));
    if (s.length() && ssids.indexOf("value=\"" + s + "\"") < 0) ssids += "<option value=\"" + s + "\">" + s + "</option>";
  }
  if (n >= 0) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);  // odśwież w tle na następne wejście
  }
  String srv = htmlEscape(identity::server().c_str());
  String h = kPageHead;
  h += "<h1>LUMI " + String(identity::id()) + "</h1><p>Podaj domową sieć Wi-Fi. LUMI zapamięta ją i połączy się sama.</p>"
       "<form method=post action=/save><label>Sieć</label><select name=s>" + ssids +
       "</select><small>Nie widać sieci? Wpisz nazwę:</small><input name=s2 autocomplete=off>"
       "<label>Hasło</label><input name=p type=password>"
       "<details><summary><small>Zaawansowane</small></summary><label>Adres serwera aplikacji</label>"
       "<input name=srv value=\"" + srv + "\"></details><button>Zapisz</button></form>";
  web->send(200, "text/html; charset=utf-8", h);
}

void pageSave() {
  String ssid = web->arg("s2").length() ? web->arg("s2") : web->arg("s");
  String srv = web->arg("srv");
  if (!ssid.length()) {
    web->send(400, "text/plain; charset=utf-8", "Brak nazwy sieci");
    return;
  }
  identity::addWifi(ssid.c_str(), web->arg("p").c_str());
  if (srv.length()) identity::setServer(srv.c_str());
  String h = kPageHead;
  h += "<h1>Zapisano</h1><p>LUMI łączy się teraz z siecią <b>" + htmlEscape(ssid) +
       "</b>. Wróć do zwykłego Wi-Fi w telefonie i zeskanuj kod z ekranu LUMI, żeby dodać je do aplikacji.</p>";
  web->send(200, "text/html; charset=utf-8", h);
  provisionSaved = true;
  provisionUntil = millis() + 3000;  // daj czas na wysłanie odpowiedzi
}

// Wyłącz radio - chyba że działa sieć panelu, wtedy tylko rozłącz z siecią domową
void radioOff() {
  if (panelApOn) {
    WiFi.disconnect(false);
    return;
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiOff = true;
  apUp = false;
}

void ensurePanelAp() {
  if (web) return;  // trwa konfiguracja Wi-Fi z telefonu - ona ma pierwszeństwo
  if (!panelApOn) {
    if (apUp) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(wifiOff ? WIFI_OFF : WIFI_STA);
      apUp = false;
    }
    return;
  }
  if (apUp && (WiFi.getMode() & WIFI_MODE_AP)) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(PANEL_SSID, identity::apPassword());
  wifiOff = false;
  apUp = true;
  Serial.printf("[panel] siec Wi-Fi \"%s\", haslo %s, potem http://%s:8080\n", PANEL_SSID, identity::apPassword(),
                WiFi.softAPIP().toString().c_str());
}

void startAp() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(identity::id(), identity::apPassword());
  WiFi.scanNetworks(true);
  dns = new DNSServer();
  dns->start(53, "*", WiFi.softAPIP());  // każda strona prowadzi do formularza
  web = new WebServer(80);
  web->on("/", HTTP_GET, pageRoot);
  web->on("/save", HTTP_POST, pageSave);
  web->onNotFound(pageRoot);
  web->begin();
  provisionSaved = false;
  provisionUntil = millis() + PROVISION_WINDOW_MS;
  wifiOff = false;
  setCloud(Cloud::Provisioning);
  Serial.printf("[wifi] siec konfiguracyjna %s, haslo %s, strona http://192.168.4.1\n", identity::id(),
                identity::apPassword());
}

void stopAp() {
  if (web) {
    web->stop();
    delete web;
    web = nullptr;
  }
  if (dns) {
    dns->stop();
    delete dns;
    dns = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apUp = false;  // sieć panelu (jeśli włączona) wróci w następnym obiegu
  cfgDirty = true;  // nowe sieci
  connectingSince = millis();
  setCloud(Cloud::Connecting);
}

// ---------- serwer aplikacji ----------

struct Resp {
  int code = -1;
  String body;
  long inbox = -1;  // X-Lumi-Inbox: id najnowszej nieprzeczytanej wiadomości (0 = brak), -1 = serwer nie podał
};

// Wiadomości pobieramy tylko, gdy serwer zgłasza nowszą niż ostatnio widziana (starszy serwer: zawsze)
long inboxTop = -1;
long newestSeen = 0;

// Jedno połączenie HTTPS na cały cykl synchronizacji (konfiguracja, wiadomości, zdarzenia, status idą
// jedno po drugim). Uzgadnianie TLS to ~1 s pracy procesora i radia - bez tego robiliśmy je 2-4 razy
// co 20 s. Po cyklu połączenie jest zamykane (closeConnection), żeby nie trzymać ~40 KB pamięci TLS
// w bezczynności i nie liczyć na to, jak długo serwer trzyma bezczynne połączenie.
WiFiClientSecure tls;
HTTPClient http;
bool tlsReady = false;

void closeConnection() {
  if (!tlsReady) return;
  http.end();
  tls.stop();
}

Resp requestOnce(const char* method, const std::string& url, const String& body, int version) {
  Resp r;
  bool https = url.rfind("https://", 0) == 0;
  http.setTimeout(10000);
  http.setReuse(true);
  if (https) {
    if (!tlsReady) {
      tls.setCACertBundle(lumi_ca_bundle);  // weryfikacja certyfikatu serwera
      tlsReady = true;
    }
    if (!http.begin(tls, url.c_str())) return r;
  } else {
#ifdef LUMI_ALLOW_HTTP
    static WiFiClient plain;
    tlsReady = true;  // closeConnection zamknie też zwykłe połączenie
    if (!http.begin(plain, url.c_str())) return r;
#else
    Serial.println("[serwer] tylko HTTPS (LUMI_ALLOW_HTTP pozwala na HTTP do testow w sieci domowej)");
    return r;
#endif
  }
  http.addHeader("Authorization", String("Bearer ") + identity::secret());
  http.addHeader("X-Lumi-Device", identity::id());
  http.addHeader("X-Lumi-Version", String(version));
  http.addHeader("Content-Type", "application/json");
  static const char* kCollect[] = {"X-Lumi-Inbox"};
  http.collectHeaders(kCollect, 1);
  r.code = strcmp(method, "GET") == 0 ? http.GET() : http.POST(body);
  if (r.code > 0) r.body = http.getString();
  if (http.hasHeader("X-Lumi-Inbox")) r.inbox = http.header("X-Lumi-Inbox").toInt();
  http.end();  // przy setReuse połączenie zostaje otwarte, jeśli serwer na to pozwala
  return r;
}

Resp request(const char* method, const char* path, const String& body, int version) {
  std::string base = identity::server();
  if (base.empty()) return Resp{};
  std::string url = base + path;
  Resp r = requestOnce(method, url, body, version);
  if (r.code < 0) {
    // serwer mógł zamknąć połączenie, które próbowaliśmy użyć ponownie: jeszcze raz, od nowa
    closeConnection();
    r = requestOnce(method, url, body, version);
  }
  return r;
}

// Usunięcie z aplikacji: serwer mówi "nikt nie dodał tego LUMI", a my mamy ustawienia z
// poprzedniego parowania. Kasujemy je dopiero, gdy słyszymy to kilka razy z rzędu przez co
// najmniej minutę - pojedyncza dziwna odpowiedź nie może wyczyścić seniorowi kontaktów.
constexpr uint8_t UNPAIR_HITS = 3;
constexpr uint32_t UNPAIR_AFTER_MS = 60 * 1000UL;
uint8_t unclaimedHits = 0;
uint32_t unclaimedSince = 0;

void noteClaimed(bool claimed, const Settings& s) {
  if (claimed || s.version == 0) {
    unclaimedHits = 0;
    return;
  }
  if (unclaimedHits == 0) unclaimedSince = millis();
  if (unclaimedHits < 255) unclaimedHits++;
  if (unclaimedHits >= UNPAIR_HITS && millis() - unclaimedSince >= UNPAIR_AFTER_MS) {
    unclaimedHits = 0;
    diag::error("[serwer] LUMI usunieto z aplikacji - powrot do parowania");
    bus::post(Event(Ev::Unpaired));
  }
}

void hello(const Settings& s) {
  JsonDocument d;
  d["id"] = identity::id();
  d["secret"] = identity::secret();
  d["pair"] = identity::pairCode();
  d["variant"] = LUMI_VARIANT;
  d["fw"] = LUMI_FW_VERSION;
  d["build"] = LUMI_FW_BUILD;
  String b;
  serializeJson(d, b);
  Resp r = request("POST", "/api/device/hello", b, s.version);
  if (r.code == 200) {
    helloDone = true;
    JsonDocument o;
    bool parsed = deserializeJson(o, r.body) == DeserializationError::Ok;
    bool claimed = parsed && (o["claimed"] | false);
    if (parsed) noteClaimed(claimed, s);  // nieczytelna odpowiedź nic nie mówi o parowaniu
    setCloud(claimed ? Cloud::Ok : Cloud::Unclaimed);
  } else if (r.code == 401 || r.code == 403) {
    setCloud(Cloud::AuthError);
    diag::error("[serwer] sekret odrzucony");
  } else {
    setCloud(Cloud::Offline);
    diag::error("[serwer] hello: %d", r.code);
  }
}

void syncConfig(const Settings& s) {
  Resp r = request("GET", "/api/device/config", "", s.version);
  if (r.code > 0) inboxTop = r.inbox;
  if (r.code == 304) {
    noteClaimed(true, s);
    setCloud(Cloud::Ok);
    return;
  }
  if (r.code == 409) {
    noteClaimed(false, s);
    setCloud(Cloud::Unclaimed);
    return;
  }
  if (r.code == 404) {  // inny serwer albo zła ścieżka - nie traktujemy jako usunięcia
    setCloud(Cloud::Unclaimed);
    return;
  }
  if (r.code == 401 || r.code == 403) {
    setCloud(Cloud::AuthError);
    helloDone = false;
    return;
  }
  if (r.code != 200) {
    setCloud(Cloud::Offline);
    return;
  }
  noteClaimed(true, s);
  setCloud(Cloud::Ok);
  // sprawdzamy dokument tym samym parserem co przy starcie - zły plik nie trafi na dysk
  cfg::Config test;
  std::string err;
  if (!cfg::parse(r.body.c_str(), r.body.length(), test, err)) {
    diag::error("[serwer] odrzucona konfiguracja: %s", err.c_str());
    return;
  }
  if (test.version <= s.version) return;
  if (saveConfigFile(r.body.c_str(), r.body.length())) {
    Serial.printf("[serwer] nowa konfiguracja v%d\n", test.version);
    bus::post(Event(Ev::ConfigUpdated, test.version));
  }
}

void report(const Settings& s) {
  modem::Status m = modem::status();
  JsonDocument d;
  d["version"] = s.version;
  d["variant"] = LUMI_VARIANT;
  d["fw"] = LUMI_FW_VERSION;
  d["build"] = LUMI_FW_BUILD;
  d["reset"] = diag::resetReason();
  d["boots"] = diag::bootCount();
  JsonArray errs = d["errors"].to<JsonArray>();
  for (const auto& e : diag::recent()) errs.add(e);
  if (battery::known()) {  // bez pomiaru pole zostaje puste - aplikacja pokaże "brak danych"
    d["battery"] = battery::percent();
    d["batteryMv"] = battery::millivolts();
  }
  d["charging"] = battery::charging();
  d["signal"] = m.bars;
  d["registered"] = m.registered;
  d["lte"] = m.lte;
  d["operator"] = m.op;
  d["modem"] = modem::vendorName(m.vendor);
  d["sim"] = m.simOk;
  d["wifiRssi"] = WiFi.RSSI();
  d["uptime"] = millis() / 1000;
  String b;
  serializeJson(d, b);
  Resp r = request("POST", "/api/device/status", b, s.version);
  if (r.code != 200) diag::error("[serwer] status: %d", r.code);
}

// ---------- zdjęcia kontaktów ----------

volatile bool photosDirty = true;

// Pobiera brakujące zdjęcia i usuwa te, których nie używa już żaden kontakt
void syncPhotos(const Settings& s) {
  if (!photosDirty) return;
  LittleFS.mkdir("/ph");
  bool changed = false, allOk = true;
  for (const auto& h : s.photos) {
    std::string path = photoPath(h);
    if (LittleFS.exists(path.c_str())) continue;
    std::string url = identity::server() + "/api/device/photo?h=" + h;
    WiFiClientSecure secure;
    WiFiClient plain;
    HTTPClient http;
    http.setTimeout(10000);
    bool https = url.rfind("https://", 0) == 0;
    if (https) secure.setCACertBundle(lumi_ca_bundle);
    if (!(https ? http.begin(secure, url.c_str()) : http.begin(plain, url.c_str()))) continue;
    http.addHeader("Authorization", String("Bearer ") + identity::secret());
    http.addHeader("X-Lumi-Device", identity::id());
    int code = http.GET();
    if (code == 200 && (size_t)http.getSize() == PHOTO_BYTES) {
      File f = LittleFS.open("/ph/tmp", "w");
      int n = f ? http.writeToStream(&f) : -1;
      f.close();
      if (n == (int)PHOTO_BYTES && LittleFS.rename("/ph/tmp", path.c_str())) changed = true;
      else {
        LittleFS.remove("/ph/tmp");
        allOk = false;
      }
    } else {
      allOk = false;
      diag::error("[zdjecia] %s: HTTP %d", h.c_str(), code);
    }
    http.end();
  }
  File dir = LittleFS.open("/ph");
  std::vector<std::string> stale;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    std::string name = f.name();  // samo "abc.565"
    std::string hash = name.substr(0, name.find('.'));
    bool used = false;
    for (const auto& h : s.photos) used |= h == hash;
    if (!used) stale.push_back("/ph/" + name);
  }
  for (const auto& p : stale) LittleFS.remove(p.c_str());
  photosDirty = !allOk;  // nieudane spróbujemy przy następnym obiegu
  if (changed) bus::post(Event(Ev::PhotosUpdated));
}

// ---------- zdarzenia ----------

// ---------- wiadomości ----------

std::vector<Message> inbox;  // chronione przez mtx
std::set<int> seenMessages;

void fetchMessages(const Settings& s) {
  Resp r = request("GET", "/api/device/messages", "", s.version);
  if (r.code != 200) return;
  JsonDocument d;
  if (deserializeJson(d, r.body)) return;
  bool any = false;
  xSemaphoreTake(mtx, portMAX_DELAY);
  for (JsonObject o : d.as<JsonArray>()) {
    int id = o["id"] | 0;
    if (!id || seenMessages.count(id)) continue;
    seenMessages.insert(id);
    if (id > newestSeen) newestSeen = id;
    inbox.push_back({id, o["author"] | "", o["text"] | "", o["at"] | 0LL});
    any = true;
  }
  if (seenMessages.size() > 200) seenMessages.erase(seenMessages.begin());
  xSemaphoreGive(mtx);
  if (any) bus::post(Event(Ev::MessagesArrived));
}

// Kolejka zdarzeń przeżywa restart: "bez potwierdzenia" po zaniku zasilania nie może przepaść, bo
// serwer go nie odtworzy (po restarcie LUMI znów jest w sieci). Kilka wpisów dziennie - flash to zniesie.
constexpr const char* EVENTS_FILE = "/events.txt";

void saveEvents() {  // wywoływać z zajętym mtx
  if (events.empty()) {
    LittleFS.remove(EVENTS_FILE);
    return;
  }
  File f = LittleFS.open("/events.tmp", "w");
  if (!f) return;
  for (const auto& e : events) {
    f.write(reinterpret_cast<const uint8_t*>(e.data()), e.size());
    f.write('\n');
  }
  f.close();
  LittleFS.rename("/events.tmp", EVENTS_FILE);
}

void loadEvents() {
  File f = LittleFS.open(EVENTS_FILE, "r");
  if (!f) return;
  while (f.available() && events.size() < MAX_EVENTS) {
    String l = f.readStringUntil('\n');
    l.trim();
    if (l.length()) events.push_back(l.c_str());
  }
  f.close();
  if (!events.empty()) Serial.printf("[serwer] %u zdarzen z poprzedniego uruchomienia czeka na wyslanie\n", (unsigned)events.size());
}

void flushEvents(const Settings& s) {
  for (int i = 0; i < 5; i++) {  // kilka na obieg, żeby nie blokować reszty
    xSemaphoreTake(mtx, portMAX_DELAY);
    if (events.empty()) {
      xSemaphoreGive(mtx);
      return;
    }
    String body = events.front().c_str();
    xSemaphoreGive(mtx);
    Resp r = request("POST", "/api/device/event", body, s.version);
    if (r.code != 200 && r.code != 400) return;  // spróbuj później; 400 = zły wpis, odrzuć
    xSemaphoreTake(mtx, portMAX_DELAY);
    if (!events.empty()) events.pop_front();
    saveEvents();
    xSemaphoreGive(mtx);
  }
}

// ---------- aktualizacje ----------

void checkOffer(const Settings& s) {
  Resp r = request("GET", "/api/device/firmware", "", s.version);
  if (r.code != 200) {
    if (r.code != 204) diag::error("[ota] sprawdzenie: %d", r.code);
    return;
  }
  JsonDocument d;
  if (deserializeJson(d, r.body)) return;
  ota::Offer o;
  o.build = d["build"] | 0;
  o.version = d["version"] | "";
  o.url = d["url"] | "";
  o.sha256 = d["sha256"] | "";
  o.sig = d["sig"] | "";
  o.size = d["size"] | 0;
  if (o.build > LUMI_FW_BUILD) {
    offer = o;
    Serial.printf("[ota] dostepna wersja %s (build %d)\n", o.version.c_str(), o.build);
  }
}

bool goodTimeToUpdate() {
  time_t t = time(nullptr);
  if (t < 1700000000) return false;
  tm l;
  localtime_r(&t, &l);
  bool night = l.tm_hour >= 2 && l.tm_hour < 5;
  bool power = battery::charging() || !battery::known() || battery::percent() >= 50;  // nieznana = zasilacz (prototyp)
  return night && power && app::idleForUpdate();
}

void maybeUpdate(const Settings& s) {
  uint32_t now = millis();
  if (otaForce || !lastOtaCheck || now - lastOtaCheck > OTA_CHECK_EVERY_MS) {
    lastOtaCheck = now;
    checkOffer(s);
  }
  if (!offer.build || !(otaForce || goodTimeToUpdate())) return;
  otaForce = false;
  std::string err;
  if (!ota::install(offer, err)) {
    diag::error("[ota] build %d: %s", offer.build, err.c_str());
    offer = ota::Offer{};
    return;
  }
  // restart dopiero, gdy nikt nie rozmawia (rozmowa może zacząć się w trakcie pobierania)
  while (app::inCall()) vTaskDelay(pdMS_TO_TICKS(1000));
  vTaskDelay(pdMS_TO_TICKS(500));
  ESP.restart();
}

// ---------- pogoda ----------

int todayKey() {
  time_t t = time(nullptr);
  if (t < 1700000000) return -1;  // zegar jeszcze nieustawiony
  tm l;
  localtime_r(&t, &l);
  return (l.tm_year + 1900) * 1000 + l.tm_yday;
}

void fetchWeather(const Settings& s) {
  char url[256];
  snprintf(url, sizeof url,
           "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
           "&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&timezone=Europe%%2FWarsaw&forecast_days=2",
           s.lat, s.lon);
  // HTTPS: w adresie są współrzędne domu seniora - nie mogą iść otwartym tekstem
  WiFiClientSecure secure;
  secure.setCACertBundle(lumi_ca_bundle);
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(secure, url)) return;
  int code = http.GET();
  if (code != 200) {
    diag::error("[pogoda] HTTP %d", code);
    http.end();
    return;
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  if (e) return;
  wx::Forecast f;
  f.tempNow = doc["current"]["temperature_2m"] | 0.0f;
  f.codeNow = doc["current"]["weather_code"] | 0;
  JsonObject dd = doc["daily"];
  for (int i = 0; i < 2; i++) {
    wx::Day& day = i == 0 ? f.today : f.tomorrow;
    day.code = dd["weather_code"][i] | 0;
    day.tMax = dd["temperature_2m_max"][i] | 0.0f;
    day.tMin = dd["temperature_2m_min"][i] | 0.0f;
    day.valid = !dd["weather_code"][i].isNull();
  }
  f.valid = !doc["current"].isNull();
  f.fetchedAt = millis();
  f.dayKey = todayKey();
  xSemaphoreTake(mtx, portMAX_DELAY);
  fc = f;
  xSemaphoreGive(mtx);
  bus::post(Event(Ev::WeatherUpdated));
}

// ---------- zadanie ----------

void task(void*) {
  WiFi.mode(WIFI_OFF);
  connectingSince = millis();
  for (;;) {
    if (web) {  // tryb konfiguracji Wi-Fi
      dns->processNextRequest();
      web->handleClient();
      if ((int32_t)(millis() - provisionUntil) >= 0) {
        stopAp();
        if (!provisionSaved) Serial.println("[wifi] koniec okna konfiguracji, probuje znanych sieci");
      }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    ensurePanelAp();
    Settings s = snapshot();
    xSemaphoreTake(mtx, portMAX_DELAY);
    bool dirty = cfgDirty;
    cfgDirty = false;
    xSemaphoreGive(mtx);
    if (dirty) {
      rebuildWifi(s);
      listOnFail = true;
      wifiFails = 0;  // nowa sieć od rodziny: próbuj od razu
      wifiRetryAt = 0;
    }

    std::vector<cfg::Wifi> nets = allNetworks(s);
    bool unconfigured = s.version == 0;
    if (provisionReq || nets.empty()) {
      provisionReq = false;
      if (nets.empty()) setCloud(Cloud::NoWifi);
      startAp();
      continue;
    }

    // Cisza elektryczna: gra radio FM, a urządzenie jest już skonfigurowane
    if (audio::fmActive() && !unconfigured) {
      if (!wifiOff) radioOff();
      continue;
    }
    if (wifiRetryAt && (int32_t)(millis() - wifiRetryAt) < 0) continue;  // przerwa między próbami
    wifiRetryAt = 0;
    if (wifiOff) {
      WiFi.mode(panelApOn ? WIFI_AP_STA : WIFI_STA);
      wifiOff = false;
      if (unconfigured) connectingSince = millis();
    }
    if (multi->run(5000) != WL_CONNECTED) {
      setCloud(Cloud::Connecting);
      if (listOnFail) {
        listOnFail = false;
        listVisibleNetworks(nets);
      }
      if (unconfigured) {
        if (millis() - connectingSince > PROVISION_AFTER_MS) startAp();  // np. złe hasło
        continue;
      }
      if (wifiFails < 255) wifiFails++;
      uint32_t wait = wifiBackoffMs(wifiFails);
      if (wait) {
        if (wifiFails == WIFI_QUICK_TRIES + 1) Serial.println("[wifi] brak znanej sieci - kolejne proby coraz rzadziej");
        radioOff();
        wifiRetryAt = millis() + wait;
      }
      continue;
    }
    if (wifiFails > WIFI_QUICK_TRIES) Serial.printf("[wifi] polaczono z %s\n", WiFi.SSID().c_str());
    wifiFails = 0;
    connectingSince = millis();

    if (!ntpStarted) {
      // Synchronizacja co godzinę (domyślnie w SNTP); każda poprawia ekran i zegar z baterią
      sntp_set_time_sync_notification_cb([](timeval*) { ntpSynced = true; });
      configTzTime(TZ_PL, "pool.ntp.org", "time.google.com");
      ntpStarted = true;
    }
    if (ntpSynced.exchange(false)) {
      bus::post(Event(Ev::NetTime, 0, 0, "ntp"));
      rtc::saveNow();
    }

    uint32_t now = millis();
    if (identity::server().empty()) {
      setCloud(Cloud::NoServer);
    } else {
      if (!helloDone && (now - lastSync > 15000 || !lastSync)) {
        lastSync = now;
        hello(s);
      }
      // parowanie: co 5 s; LUMI śpi na baterii: najwyżej co minutę (każde połączenie TLS kosztuje prąd)
      uint32_t sec = s.syncIntervalSec;
      if (app::idleForUpdate() && !battery::charging() && sec < 60) sec = 60;
      uint32_t every = (cloudState == Cloud::Unclaimed ? 5 : sec) * 1000UL;
      if (helloDone && (!lastSync || now - lastSync > every)) {
        lastSync = now;
        syncConfig(s);
        msgDue = inboxTop < 0 || inboxTop > newestSeen;
      }
      if (helloDone && cloudState == Cloud::Ok && (reportReq || now - lastReport > REPORT_EVERY_MS)) {
        reportReq = false;
        lastReport = now;
        report(snapshot());
      }
    }
    if (helloDone && cloudState == Cloud::Ok) flushEvents(s);
    if (helloDone && cloudState == Cloud::Ok && msgDue) {
      msgDue = false;
      fetchMessages(s);
    }
    if (helloDone && cloudState == Cloud::Ok) syncPhotos(s);
    if (helloDone && cloudState == Cloud::Ok && ota::enabled()) maybeUpdate(s);
    if (s.weather && s.version > 0 && (weatherReq || !lastWeather || now - lastWeather > WEATHER_EVERY_MS)) {
      weatherReq = false;
      lastWeather = now;
      fetchWeather(s);
    }
    closeConnection();
  }
}

}  // namespace

void begin() {
  mtx = xSemaphoreCreateMutex();
  loadEvents();
  // 12 KB: uzgadnianie TLS (mbedTLS) potrafi zająć ok. 8 KB stosu; zapas widać w "status"
  xTaskCreatePinnedToCore(task, "net", 12288, nullptr, 2, nullptr, 0);
}

void apply(const Settings& s) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool coordsChanged = s.lat != cfgNet.lat || s.lon != cfgNet.lon;
  bool versionChanged = s.version != cfgNet.version;
  cfgNet = s;
  cfgDirty = true;
  xSemaphoreGive(mtx);
  if (coordsChanged) weatherReq = true;
  if (versionChanged) reportReq = true;  // rodzina widzi w aplikacji, że zmiana dotarła
  if (versionChanged) photosDirty = true;
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
std::string wifiSsid() { return WiFi.SSID().c_str(); }
Cloud cloud() { return cloudState; }

const char* cloudName(Cloud c) {
  switch (c) {
    case Cloud::NoWifi: return "brak sieci Wi-Fi";
    case Cloud::Provisioning: return "konfiguracja Wi-Fi z telefonu";
    case Cloud::Connecting: return "laczenie z Wi-Fi";
    case Cloud::NoServer: return "brak adresu serwera";
    case Cloud::Offline: return "serwer nie odpowiada";
    case Cloud::Unclaimed: return "czeka na dodanie w aplikacji";
    case Cloud::Ok: return "polaczone z aplikacja";
    case Cloud::AuthError: return "serwer odrzuca klucz";
  }
  return "?";
}

wx::Forecast forecast() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  wx::Forecast f = fc;
  xSemaphoreGive(mtx);
  return wx::fresh(f, millis(), todayKey());
}

void refreshWeather() { weatherReq = true; }
void reportSoon() { reportReq = true; }
void startProvisioning() { provisionReq = true; }
void checkUpdateNow() { otaForce = true; }
void setPanelAp(bool on) { panelApOn = on; }
bool panelAp() { return panelApOn; }

std::vector<Message> takeMessages() {
  xSemaphoreTake(mtx, portMAX_DELAY);
  std::vector<Message> out;
  out.swap(inbox);
  xSemaphoreGive(mtx);
  return out;
}

void queueEvent(const std::string& json) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  if (events.size() >= MAX_EVENTS) events.pop_front();
  events.push_back(json);
  saveEvents();
  xSemaphoreGive(mtx);
}

std::string photoPath(const std::string& hash) { return "/ph/" + hash + ".565"; }

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
