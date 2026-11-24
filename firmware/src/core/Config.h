#pragma once
// Konfiguracja urządzenia: jeden dokument JSON z numerem wersji, ustawiany przez rodzinę.
// Opis pól: docs/config.md. Przykład: data/config.json.
#include <stdint.h>

#include <string>
#include <vector>

namespace cfg {

struct Contact {
  std::string id, name, number;
  uint32_t color = 0x4CC38A;  // kolor kółka z inicjałem (zdjęcia w kolejnej wersji)
  bool autoAnswer = false;    // automatyczne odbieranie od tej osoby
};

enum class MediaType : uint8_t { Fm, Web, Playlist };

struct Media {
  MediaType type = MediaType::Fm;
  std::string name;
  std::string from;               // "od Ani" - kto dodał (muzyka)
  float freq = 0;                 // MHz, dla FM
  std::vector<std::string> urls;  // strumień lub lista plików MP3
};

enum class SlotKind : uint8_t { None, Contact, Media, Weather };

struct HomeSlot {
  SlotKind kind = SlotKind::None;
  std::string contactId;
};

struct Wifi {
  std::string ssid, pass;
};

struct Config {
  int version = 0;
  std::string changedBy;   // "Ania"
  std::string changeText;  // "dodała kontakt Basia" - aplikacja składa zdanie, urządzenie nie zgaduje rodzaju

  // funkcje włączone przez rodzinę
  bool calls = true, media = true, weather = true;

  std::vector<Contact> contacts;  // w kolejności z aplikacji (lista pod przyciskiem Dzwoń)
  bool onlyContacts = false;      // odrzucaj numery spoza listy (ochrona przed oszustami)

  HomeSlot home[3];  // trzy przyciski prawej kolumny na ekranie głównym
  bool partOfDay = true;

  std::vector<Media> mediaList;

  std::string place;
  float lat = 52.23f, lon = 21.01f;

  uint8_t textSize = 0;  // 0 normalny, 1 duży, 2 bardzo duży
  bool highContrast = false;
  uint8_t brightness = 80;  // %
  uint16_t screenOffSec = 60;

  uint8_t ringVolume = 8;   // 0..10
  uint8_t maxVolume = 10;   // limit rolki 0..10
  uint8_t startVolume = 5;  // głośność po włączeniu
  bool keyClick = true;
  int quietFrom = -1, quietTo = -1;  // minuty od północy, -1 = brak godzin ciszy

  uint8_t autoAnswerSec = 5;  // 3..10
  bool oneButton = false;     // tryb jednego przycisku
  uint16_t minPressMs = 0;    // drżenie dłoni
  uint16_t repeatIgnoreMs = 250;
  bool lockMusic = false, lockCall = false;
  bool allowPowerOff = true;

  std::vector<Wifi> wifi;
  uint16_t syncIntervalSec = 20;  // jak często pytać serwer o zmiany (Wi-Fi)

  const Contact* contactById(const std::string& id) const;
  int contactIndexByNumber(const char* number) const;  // -1 gdy nieznany
  bool inQuietHours(int minuteOfDay) const;
};

// Zwraca false i opis błędu, gdy dokument jest nieczytelny. Brakujące pola dostają wartości domyślne.
bool parse(const char* json, size_t len, Config& out, std::string& err);

uint32_t parseColor(const char* hex, uint32_t fallback);

}  // namespace cfg
