#include "app/Console.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <string>

#include "Version.h"
#include "app/App.h"
#include "app/Diag.h"
#include "audio/AudioOut.h"
#include "hw/Battery.h"
#include "hw/Input.h"
#include "modem/Modem.h"
#include "net/Identity.h"
#include "net/Ota.h"
#include "net/Net.h"

namespace console {

namespace {

char buf[256];
size_t len = 0;

bool pasting = false;  // tryb "config": wklejanie dokumentu JSON, koniec linią z kropką
std::string pasted;

void help() {
  Serial.println(
      "LUMI - konsola serwisowa\n"
      "  status            stan modemu, sieci, baterii\n"
      "  diag              komendy do testu VoLTE (etap 1)\n"
      "  at <komenda>      wyslij komende AT, np. at AT+COPS?\n"
      "  bridge            bezposrednie polaczenie z modemem (wyjscie: +++)\n"
      "  call <numer>      zadzwon; answer; hangup\n"
      "  key <nazwa> [long]  nacisnij przycisk: top mid bottom music home call power\n"
      "  wheel <n>         obrot rolki (+ glosniej, - ciszej)\n"
      "  fm <MHz>          radio FM, np. fm 98.8; url <adres> radio internetowe; stop\n"
      "  config            wklej dokument konfiguracji JSON, zakoncz linia z kropka\n"
      "  cfg               podsumowanie konfiguracji; reload - wczytaj ponownie\n"
      "  notice <tekst>    pokaz komunikat jak po zmianie od rodziny\n"
      "  wifi <nazwa> <haslo>  dodaj siec Wi-Fi; provision - siec LUMI do konfiguracji z telefonu\n"
      "  server <adres>    adres aplikacji rodziny, np. server https://lumi.vercel.app\n"
      "  pair              link i kod parowania; factory - przywroc ustawienia fabryczne\n"
      "  remind <tekst>    pokaz przypomnienie teraz (test)\n"
      "  msg <tekst>       pokaz wiadomosc od Ani (test)\n"
      "  selftest          test sprzetu krok po kroku (albo Home przytrzymany przy wlaczaniu)\n"
      "  ota               sprawdz i zainstaluj aktualizacje teraz (bez czekania na noc)");
}

void status() {
  modem::Status s = modem::status();
  Serial.printf("urzadzenie: %s (%s), oprogramowanie %s (build %d)\n", app::deviceId(), LUMI_VARIANT, LUMI_FW_VERSION,
                LUMI_FW_BUILD);
  Serial.printf("uruchomienie %u, ostatni restart: %s\n", diag::bootCount(), diag::resetReason());
  for (const auto& e : diag::recent()) Serial.printf("  blad: %s\n", e.c_str());
  Serial.printf("modem: %s, %s %s, fw %s\n", s.ready ? "gotowy" : "nie gotowy", modem::vendorName(s.vendor), s.model,
                s.fw);
  Serial.printf("SIM: %s, siec: %s, operator: %s, LTE: %s, zasieg: %d/4\n", s.simOk ? "ok" : "brak",
                s.registered ? "zarejestrowany" : "brak", s.op, s.lte ? "tak" : "nie", s.bars);
  Serial.printf("bateria: %u mV, %u%%, ladowanie: %s\n", battery::millivolts(), battery::percent(),
                battery::charging() ? "tak" : "nie");
  Serial.printf("wifi: %s %s\n", net::wifiConnected() ? "polaczone" : "brak", net::wifiSsid().c_str());
  Serial.printf("serwer: %s (%s)\n", identity::server().c_str(), net::cloudName(net::cloud()));
  audio::State a = audio::state();
  Serial.printf("audio: zrodlo %d, gra %d, glosnosc %u\n", (int)a.src, a.playing, a.volume);
  time_t t = time(nullptr);
  Serial.printf("czas: %s", ctime(&t));
  Serial.printf("wolna pamiec: %u B, PSRAM: %u B\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

void cfgSummary() {
  if (!app::configured()) {
    Serial.println("brak konfiguracji");
    return;
  }
  const cfg::Config& c = app::config();
  Serial.printf("wersja %d (zmienil: %s)\n", c.version, c.changedBy.c_str());
  Serial.printf("kontakty: %u, media: %u, wifi: %u, rozmiar tekstu: %u, kontrast: %d\n", (unsigned)c.contacts.size(),
                (unsigned)c.mediaList.size(), (unsigned)c.wifi.size(), c.textSize, c.highContrast);
  for (const auto& k : c.contacts) Serial.printf("  %s %s%s\n", k.name.c_str(), k.number.c_str(), k.autoAnswer ? " (auto)" : "");
}

void finishPaste() {
  pasting = false;
  cfg::Config test;
  std::string err;
  if (!cfg::parse(pasted.data(), pasted.size(), test, err)) {
    Serial.printf("blad: %s - konfiguracja bez zmian\n", err.c_str());
  } else if (net::saveConfigFile(pasted.data(), pasted.size())) {
    Serial.printf("zapisano wersje %d\n", test.version);
    app::reloadConfig(true);
  } else {
    Serial.println("blad zapisu");
  }
  pasted.clear();
}

void exec(char* l) {
  char* arg = strchr(l, ' ');
  if (arg) *arg++ = 0;
  else arg = l + strlen(l);

  if (!strcmp(l, "help")) help();
  else if (!strcmp(l, "status")) status();
  else if (!strcmp(l, "diag")) modem::diag();
  else if (!strcmp(l, "at")) modem::raw(arg);
  else if (!strcmp(l, "bridge")) modem::bridge(Serial);
  else if (!strcmp(l, "call")) modem::dial(arg);
  else if (!strcmp(l, "answer")) modem::answer();
  else if (!strcmp(l, "hangup")) modem::hangup();
  else if (!strcmp(l, "key")) {
    char* mode = strchr(arg, ' ');
    if (mode) *mode++ = 0;
    Key k;
    if (!input::keyFromName(arg, k)) {
      Serial.println("nieznany przycisk");
      return;
    }
    if (mode && !strcmp(mode, "long")) {
      input::inject(k, BtnEvent::HoldStart);
      input::inject(k, BtnEvent::Long);
    } else {
      input::inject(k, BtnEvent::Short);
    }
  } else if (!strcmp(l, "wheel")) bus::post(Event(Ev::Wheel, atoi(arg)));
  else if (!strcmp(l, "fm")) audio::playFm(atof(arg));
  else if (!strcmp(l, "url")) audio::playUrls({std::string(arg)});
  else if (!strcmp(l, "stop")) audio::stop();
  else if (!strcmp(l, "config")) {
    pasting = true;
    pasted.clear();
    Serial.println("wklej JSON, na koncu linia z sama kropka");
  } else if (!strcmp(l, "cfg")) cfgSummary();
  else if (!strcmp(l, "reload")) Serial.println(app::reloadConfig(true) ? "ok" : "blad");
  else if (!strcmp(l, "notice")) app::showNotice(arg);
  else if (!strcmp(l, "wifi")) {
    char* pass = strchr(arg, ' ');
    if (pass) *pass++ = 0;
    if (!*arg) {
      Serial.println("uzycie: wifi <nazwa> <haslo> (nazwa bez spacji; inaczej przez provision)");
      return;
    }
    identity::addWifi(arg, pass ? pass : "");
    app::reloadConfig(false);
    Serial.println("zapisano siec");
  } else if (!strcmp(l, "provision")) net::startProvisioning();
  else if (!strcmp(l, "server")) {
    identity::setServer(arg);
    Serial.printf("serwer: %s (zadziala po restarcie lub przy nastepnej probie)\n", identity::server().c_str());
  } else if (!strcmp(l, "pair")) {
    Serial.printf("id: %s\nkod: %s\nlink: %s/pair?d=%s&c=%s\n", identity::id(), identity::pairCode(),
                  identity::server().c_str(), identity::id(), identity::pairCode());
  } else if (!strcmp(l, "selftest")) {
    app::startSelfTest();
  } else if (!strcmp(l, "msg")) {
    app::testMessage("Ania", *arg ? arg : "Przyjadę o 15, nie gotuj obiadu");
  } else if (!strcmp(l, "remind")) {
    app::testReminder(*arg ? arg : "Weź leki");
  } else if (!strcmp(l, "ota")) {
    Serial.println(ota::enabled() ? "sprawdzam aktualizacje..." : "aktualizacje wylaczone: brak klucza (tools/gen_ota_key.py)");
    net::checkUpdateNow();
  } else if (!strcmp(l, "factory")) {
    Serial.println("przywracam ustawienia fabryczne...");
    identity::factoryReset();
    delay(200);
    ESP.restart();
  }
  else if (*l) Serial.println("nieznana komenda, wpisz help");
}

}  // namespace

void poll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = 0;
      len = 0;
      if (pasting) {
        if (!strcmp(buf, ".")) finishPaste();
        else if (pasted.size() < 32768) pasted.append(buf).append("\n");
      } else {
        exec(buf);
      }
      continue;
    }
    if (len + 1 < sizeof buf) buf[len++] = c;
  }
}

}  // namespace console
