#pragma once
// Dziennik dla twórcy: powód ostatniego restartu, licznik uruchomień i ostatnie błędy.
// Trafia do raportu stanu, żeby problem u użytkownika dało się zdiagnozować zdalnie.
// Bez treści rozmów i bez numerów telefonów (dokumentacja v2.0, "Zasady działania").
#include <string>
#include <vector>

namespace diag {

void begin();
void error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
const char* resetReason();
unsigned bootCount();
std::vector<std::string> recent();  // najnowsze na końcu

}  // namespace diag
