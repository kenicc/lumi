// LUMI Pocket - plik szkicu dla Arduino IDE.
// Cały program jest w folderze src/ (setup i loop w src/main.cpp). Ten plik musi istnieć, bo Arduino
// IDE otwiera projekt przez plik .ino o tej samej nazwie co folder. PlatformIO go pomija.
//
// Instrukcja: docs/arduino-ide.md
// Ustawienia wariantu (ekran, modem): src/BuildConfig.h

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#error "Potrzebna plytka esp32 (Espressif) w wersji 2.0.17. W Menedzerze plytek wybierz wersje 2.0.17 zamiast 3.x."
#endif
#if !defined(BOARD_HAS_PSRAM)
#error "Wlacz PSRAM: Narzedzia -> PSRAM -> OPI PSRAM"
#endif
