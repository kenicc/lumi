#pragma once
// Aktualizacje oprogramowania przez Wi-Fi (dokumentacja v2.0, "Jak urządzenie dostaje zmiany", p. 4):
// - nowa wersja zapisuje się do drugiej partycji, stara zostaje nietknięta;
// - obraz musi mieć poprawny skrót SHA-256 i podpis ECDSA P-256 twórcy (klucz publiczny
//   w ota_pubkey.h) - bez klucza aktualizacje są wyłączone;
// - jeśli nowa wersja 3 razy nie dojdzie do stanu "zdrowa" (3 min pracy), LUMI wraca do poprzedniej;
// - instalacja tylko w nocy (2:00-5:00), gdy LUMI nic nie robi i ma baterię lub ładowarkę.
#include <string>

namespace ota {

struct Offer {
  int build = 0;
  std::string version, url, sha256, sig;
  size_t size = 0;
};

void begin();        // pierwsza rzecz w setup(): liczenie prób nowej wersji i ewentualny powrót
void markHealthy();  // nowa wersja działa - nie wracaj
bool updatedThisBoot();
int rolledBackFrom();  // build, z którego wrócono (0 = nie było powrotu)
bool enabled();        // jest klucz publiczny

// Pobiera, sprawdza i zapisuje obraz. Po true trzeba zrestartować urządzenie.
bool install(const Offer& o, std::string& err);

}  // namespace ota
