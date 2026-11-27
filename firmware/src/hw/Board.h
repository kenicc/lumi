#pragma once
#include "../BuildConfig.h"
// Piny ESP32-S3 DevKitC-1 N16R8 - zgodnie z tabelą w dokumentacji v2.0.
// Przed lutowaniem sprawdź pinout swojej wersji DevKitu: v1.0 ma diodę RGB na GPIO48, v1.1 na GPIO38.
// Ustaw LUMI_DEVKIT_V10, jeśli masz v1.0.
#include <stdint.h>

namespace pins {

constexpr int BATTERY_ADC = 1;
constexpr int BTN_POWER = 2;  // budzenie z uśpienia (RTC GPIO)
constexpr int MODEM_RI = 3;

constexpr int I2C_SDA = 4;
constexpr int I2C_SCL = 5;

constexpr int I2S_BCLK = 6;
constexpr int I2S_LRCK = 7;
constexpr int I2S_DOUT = 8;

constexpr int LCD_DC = 9;
constexpr int LCD_CS = 10;
constexpr int LCD_MOSI = 11;
constexpr int LCD_SCK = 12;
constexpr int LCD_BL = 13;

constexpr int AMP_SD = 14;  // PAM8302: HIGH = gra, LOW = uśpiony

constexpr int BTN_TOP = 15;
constexpr int BTN_MID = 16;
constexpr int BTN_BOTTOM = 17;
constexpr int BTN_MUSIC = 18;
constexpr int BTN_HOME = 21;
constexpr int BTN_CALL = 39;

constexpr int WHEEL_A = 40;
constexpr int WHEEL_B = 41;

constexpr int MODEM_PWRKEY = 42;
constexpr int MODEM_TX = 47;  // ESP32 -> modem
#ifdef LUMI_DEVKIT_V10
constexpr int MODEM_RX = 38;   // pin bez diody RGB
constexpr int MODEM_DTR = 48;  // pin z diodą RGB (będzie mrugać - nie szkodzi)
#else
constexpr int MODEM_RX = 48;
constexpr int MODEM_DTR = 38;
#endif

}  // namespace pins

namespace board {

// Dzielnik napięcia baterii (2 x 100 kΩ) -> mnożnik 2.0. Zmień, jeśli masz inne rezystory.
constexpr float BATTERY_DIVIDER = 2.0f;

// Płytki z modemem różnie sterują PWRKEY: bezpośrednio (stan niski włącza) albo przez tranzystor
// (stan wysoki włącza). KAmod A7670E i większość płytek z AliExpress mają tranzystor.
#ifndef LUMI_PWRKEY_ACTIVE_LOW
constexpr bool PWRKEY_ACTIVE_HIGH = true;
#else
constexpr bool PWRKEY_ACTIVE_HIGH = false;
#endif

// Płytka modemu bez PWRKEY (SIM800L): pin 42 idzie do RST modemu, stan niski = reset.
#ifdef LUMI_MODEM_RESET_ONLY
constexpr bool MODEM_RESET_ONLY = true;
#else
constexpr bool MODEM_RESET_ONLY = false;
#endif

// Prototyp z Allegro nie ma DAC, wzmacniacza ani tunera FM: dźwięk (dzwonek, rozmowa, sygnały)
// idzie wyłącznie z głośnika podłączonego do modemu SIM800L. Radio i muzyka są wtedy niedostępne.
#ifdef LUMI_AUDIO_MODEM
constexpr bool HAS_MEDIA_AUDIO = false;
#else
constexpr bool HAS_MEDIA_AUDIO = true;
#endif

// Odwrócenie kolorów ekranu: panele IPS ST7789 zwykle wymagają, zwykłe TN różnie.
// Jeśli kolory wyglądają jak negatyw, przełącz flagę LUMI_PANEL_INVERT w platformio.ini.
#ifndef LUMI_PANEL_INVERT
#if defined(LUMI_PANEL_ST7789)
#define LUMI_PANEL_INVERT 1
#else
#define LUMI_PANEL_INVERT 0
#endif
#endif

// Obrót ekranu (0-3). Zależy od tego, jak moduł leży w obudowie.
#ifndef LUMI_ROTATION
#define LUMI_ROTATION 1
#endif

// Ile impulsów rolki na jeden "ząbek". Typowe enkodery: 4 (pełny cykl) albo 2.
constexpr int WHEEL_STEPS_PER_DETENT = 4;

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

}  // namespace board
