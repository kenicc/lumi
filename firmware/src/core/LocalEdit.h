#pragma once
// Zmiany ustawień zrobione na samym LUMI (bez telefonu): czysta logika na dokumencie konfiguracji,
// testowana w test/test_core. Zmiana to operacja ("dodaj kontakt"), a nie cały dokument, bo:
// - bez parowania trafia tylko do /config.json na urządzeniu;
// - po sparowaniu ta sama operacja idzie do serwera (zdarzenie "setting") i jest nakładana na
//   aktualne ustawienia rodziny - równoległa zmiana w aplikacji nie przepada.
// Hasła Wi-Fi tu nie ma: zostaje w urządzeniu (identity::addWifi) i nie idzie na serwer.
#include <string>

namespace local {

enum class Kind : unsigned char { AddContact, RemoveContact, TextSize, RingVolume };

struct Op {
  Kind kind = Kind::AddContact;
  std::string id;      // kontakt (dodawany albo usuwany)
  std::string name;    // AddContact
  std::string number;  // AddContact
  int value = 0;       // TextSize 0..2, RingVolume 1..10
};

constexpr size_t MAX_CONTACTS = 30;  // jak w aplikacji (app/src/lib/schema.ts)
constexpr size_t MAX_NAME = 24;

// Stosuje operację do dokumentu (pusty = nowy dokument urządzenia, wersja 0). false + err, gdy
// operacja jest nieprawidłowa; dokument wejściowy zostaje wtedy bez zmian.
bool apply(const std::string& in, const Op& op, std::string& out, std::string& err);

// Zdarzenie dla serwera: {"type":"setting","op":"addContact",...}
std::string eventJson(const Op& op);

// "+48 600-100-200" -> "+48600100200"; pusty, gdy to nie numer (3-20 cyfr, + tylko na początku)
std::string normalizeNumber(const std::string& in);

// Liczba znaków UTF-8 (imiona z polskimi literami)
size_t utf8Len(const std::string& s);

}  // namespace local
