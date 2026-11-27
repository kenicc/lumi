#pragma once
// Ustawienia wariantu, gdy projekt buduje Arduino IDE. PlatformIO podaje je w platformio.ini
// (build_flags), a Arduino IDE nie ma takiej możliwości - dlatego są tutaj.
// Inny wariant w Arduino IDE: zmień poniższe linie (np. LUMI_DEVKIT_V10 dla płytki v1.0).
#if defined(ARDUINO) && !defined(PLATFORMIO)
#ifndef LUMI_VARIANT
#define LUMI_VARIANT "allegro"
#define LUMI_PANEL_ST7789
#define LUMI_PANEL_INVERT 0
#define LUMI_MODEM_RESET_ONLY
#define LUMI_AUDIO_MODEM  // dźwięk z głośnika modemu (prototyp bez DAC i radia FM)
// #define LUMI_DEVKIT_V10
#endif
#endif
