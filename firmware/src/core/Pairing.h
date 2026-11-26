#pragma once
// Teksty kodów QR na ekranie parowania (czysta logika, testowana na komputerze).
#include <string>

namespace pairing {

// Kod QR sieci Wi-Fi (format rozumiany przez aparat Androida i iPhone'a):
// WIFI:T:WPA;S:<nazwa>;P:<hasło>;; ze znakami \ ; , : " poprzedzonymi "\".
std::string wifiQr(const std::string& ssid, const std::string& pass);

// Link otwierający stronę parowania w aplikacji rodziny: <serwer>/pair?d=<id>&c=<kod>
std::string pairUrl(const std::string& server, const std::string& id, const std::string& code);

}  // namespace pairing
