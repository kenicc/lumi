#pragma once
// Parsowanie odpowiedzi i komunikatów modemu (SIMCom A7670E i Quectel EG915U).
// Tylko standardowe komendy 3GPP, żeby obie rodziny modemów działały tym samym kodem.
#include <stddef.h>
#include <stdint.h>

namespace at {

// "+CLIP: "+48600100200",145,..." -> numer dzwoniącego (pusty, gdy zastrzeżony)
bool parseClip(const char* line, char* number, size_t n);

struct Clcc {
  int idx = 0;
  int dir = 0;   // 0 wychodząca, 1 przychodząca
  int stat = 0;  // 0 aktywna, 1 zawieszona, 2 wybieranie, 3 dzwoni u rozmówcy, 4 przychodząca, 5 oczekująca
  int mode = 0;  // 0 głos
  char number[24] = {0};
};
// "+CLCC: 1,1,4,0,0,"+48600100200",145"
bool parseClcc(const char* line, Clcc& out);

// "+CSQ: 20,99" -> rssi 0..31, 99 = nieznany
bool parseCsq(const char* line, int& rssi);
// rssi -> kreski 0..4 (0 = brak zasięgu)
int csqToBars(int rssi);

// "+CREG: 0,1" / "+CEREG: 0,5" (odpowiedź na zapytanie przy n=0) -> stat
bool parseRegStat(const char* line, int& stat);
inline bool isRegistered(int stat) { return stat == 1 || stat == 5; }

// "+COPS: 0,0,"Plus",7" -> nazwa operatora i technologia (7 = LTE)
bool parseCops(const char* line, char* op, size_t n, int& act);

// "+CCLK: "26/11/24,22:31:05+04"" -> czas UTC (sekundy od 1970). Strefa w kwadransach.
bool parseCclk(const char* line, int64_t& utc);

// "+CBC: 0,76,3987" -> napięcie w mV (SIM800)
bool parseCbc(const char* line, int& mv);

// Porównanie numerów niezależnie od zapisu (+48, 0048, 48, spacje): pełny numer z kierunkowym,
// numer bez kierunkowego traktowany jako polski.
bool sameNumber(const char* a, const char* b);

// "600100200" -> "600 100 200" (czytelniej dla seniora)
void formatNumber(const char* in, char* out, size_t n);

int64_t daysFromCivil(int y, unsigned m, unsigned d);

// Książka telefoniczna karty SIM: "+CPBR: 1,"600100200",129,"Ania"". Przy AT+CSCS="UCS2" modem podaje
// imię (a SIM800 także numer) jako szesnastkowy UCS2 - dekodujemy do UTF-8 (polskie litery).
// ucs2 = modem przyjął AT+CSCS="UCS2" (bez tego "ABBA" albo "60010020" wyglądałyby jak hex).
bool parseCpbr(const char* line, bool ucs2, char* number, size_t nn, char* name, size_t nm);
// "0041006E0069006100" -> "Ania"; tekst, który nie jest UCS2 w hex, zostaje bez zmian
void ucs2Decode(const char* in, char* out, size_t n);
// "+CPBR: (1-250),40,14" -> największy indeks
bool parseCpbrRange(const char* line, int& last);

}  // namespace at
