#include "core/Pairing.h"

namespace pairing {

static std::string esc(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') o += '\\';
    o += c;
  }
  return o;
}

std::string wifiQr(const std::string& ssid, const std::string& pass) {
  return "WIFI:T:WPA;S:" + esc(ssid) + ";P:" + esc(pass) + ";;";
}

std::string pairUrl(const std::string& server, const std::string& id, const std::string& code) {
  std::string base = server;
  while (!base.empty() && base.back() == '/') base.pop_back();
  return base + "/pair?d=" + id + "&c=" + code;
}

}  // namespace pairing
