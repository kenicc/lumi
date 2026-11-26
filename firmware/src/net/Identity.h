#pragma once
// Tożsamość urządzenia i ustawienia sieci zapisane w NVS (nie w pliku konfiguracji):
// - id "LUMI-XXXXXX" z adresu MAC,
// - sekret (64 znaki hex) - urządzenie podpisuje nim każde zapytanie do serwera,
// - kod parowania (8 znaków) - pokazywany tylko na ekranie LUMI, w kodzie QR,
// - hasło sieci LUMI do konfiguracji Wi-Fi z telefonu,
// - adres serwera i sieci Wi-Fi dodane z telefonu przed pierwszą konfiguracją.
// Produkcja: sekret powinien powstawać fabrycznie, a NVS być szyfrowany (doc v2.0, punkt 1).
#include <string>
#include <vector>

#include "core/Config.h"

namespace identity {

void begin();
const char* id();
const char* secret();
const char* pairCode();
const char* apPassword();

std::string server();  // bez końcowego "/"
void setServer(const std::string& url);

std::vector<cfg::Wifi> wifi();  // sieci dodane z telefonu lub konsoli
void addWifi(const std::string& ssid, const std::string& pass);

void factoryReset();  // nowy sekret i kod, usuwa sieci i konfigurację

}  // namespace identity
