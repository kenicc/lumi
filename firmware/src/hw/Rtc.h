#pragma once
// Zegar z baterią DS3231 (opcjonalny, I2C na pinach 4/5): godzina od razu po włączeniu, także bez
// internetu i zasięgu. Bez modułu wszystko działa jak dotąd (czas z Wi-Fi albo z sieci komórkowej).
namespace rtc {

void begin();      // przy starcie: jeśli moduł jest i ma prawidłowy czas - ustawia zegar systemowy
bool present();
void saveNow();    // po synchronizacji z siecią: zapisz dokładny czas do modułu (bezpieczne z każdego zadania)

}  // namespace rtc
