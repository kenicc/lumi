#include "LocalEdit.h"

#include <ArduinoJson.h>

namespace local {

namespace {

// te same kolory co w aplikacji (CONTACT_COLORS) - kontakt dodany na LUMI wygląda tak samo
const char* const COLORS[] = {"#3D8BFD", "#C2185B", "#2E7D32", "#6D4C41", "#7B1FA2", "#E65100", "#00838F", "#5D4037"};
constexpr size_t NCOLORS = sizeof(COLORS) / sizeof(COLORS[0]);

const char* opName(Kind k) {
  switch (k) {
    case Kind::AddContact: return "addContact";
    case Kind::RemoveContact: return "removeContact";
    case Kind::TextSize: return "textSize";
    case Kind::RingVolume: return "ringVolume";
  }
  return "";
}

}  // namespace

size_t utf8Len(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) n++;
  return n;
}

std::string normalizeNumber(const std::string& in) {
  std::string out;
  for (size_t i = 0; i < in.size(); i++) {
    char c = in[i];
    if (c >= '0' && c <= '9') out += c;
    else if (c == '+' && out.empty()) out += c;
    else if (c == ' ' || c == '-' || c == '(' || c == ')') continue;
    else return "";
  }
  size_t digits = out.size() - (out.size() && out[0] == '+' ? 1 : 0);
  return digits >= 3 && digits <= 20 ? out : "";
}

bool apply(const std::string& in, const Op& op, std::string& out, std::string& err) {
  JsonDocument doc;
  if (!in.empty()) {
    if (deserializeJson(doc, in) || !doc.is<JsonObject>()) return err = "zly dokument", false;
  } else {
    doc["version"] = 0;  // urządzenie bez aplikacji
  }
  switch (op.kind) {
    case Kind::AddContact: {
      std::string num = normalizeNumber(op.number);
      if (num.empty()) return err = "zly numer", false;
      size_t len = utf8Len(op.name);
      if (!len || len > MAX_NAME) return err = "zle imie", false;
      if (op.id.empty()) return err = "brak id", false;
      JsonArray c = doc["contacts"].is<JsonArray>() ? doc["contacts"].as<JsonArray>() : doc["contacts"].to<JsonArray>();
      for (JsonObject k : c) {
        if (k["id"] == op.id) return err = "takie id juz jest", false;
        if (normalizeNumber(k["number"] | "") == num) return err = "ten numer juz jest", false;
      }
      if (c.size() >= MAX_CONTACTS) return err = "za duzo kontaktow", false;
      const char* color = COLORS[c.size() % NCOLORS];  // kolejny kolor, jak w aplikacji
      JsonObject k = c.add<JsonObject>();
      k["id"] = op.id;
      k["name"] = op.name;
      k["number"] = num;
      k["color"] = color;
      k["autoAnswer"] = false;
      break;
    }
    case Kind::RemoveContact: {
      JsonArray c = doc["contacts"];
      bool found = false;
      for (size_t i = 0; c && i < c.size(); i++)
        if (c[i]["id"] == op.id) {
          c.remove(i);
          found = true;
          break;
        }
      if (!found) return err = "nie ma takiego kontaktu", false;
      // skrót na ekranie głównym do usuniętej osoby nie może zostać
      JsonArray slots = doc["home"]["slots"];
      std::string slot = "contact:" + op.id;
      for (size_t i = 0; slots && i < slots.size(); i++)
        if (slots[i] == slot) slots[i] = "";
      break;
    }
    case Kind::TextSize:
      if (op.value < 0 || op.value > 2) return err = "zla wielkosc", false;
      doc["display"]["textSize"] = op.value;
      break;
    case Kind::RingVolume:
      if (op.value < 1 || op.value > 10) return err = "zla glosnosc", false;
      doc["sound"]["ringVolume"] = op.value;
      break;
  }
  // komunikat "zmiana od rodziny" dotyczył poprzedniej wersji - tej zmiany senior dokonał sam
  doc["changedBy"] = "";
  doc["changeText"] = "";
  out.clear();
  serializeJson(doc, out);
  return true;
}

std::string eventJson(const Op& op) {
  JsonDocument d;
  d["type"] = "setting";
  d["op"] = opName(op.kind);
  switch (op.kind) {
    case Kind::AddContact:
      d["id"] = op.id;
      d["name"] = op.name;
      d["number"] = normalizeNumber(op.number);
      break;
    case Kind::RemoveContact: d["id"] = op.id; break;
    case Kind::TextSize:
    case Kind::RingVolume: d["value"] = op.value; break;
  }
  std::string s;
  serializeJson(d, s);
  return s;
}

}  // namespace local
