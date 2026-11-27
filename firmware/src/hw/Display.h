#pragma once
// Ekran SPI 320x240 przez LovyanGFX + sterownik LVGL.
// proto: 2,4" ILI9341, series: 2,8" IPS ST7789 (wybór flagą w platformio.ini).
#include <stdint.h>

namespace display {

void begin();
void tick();  // lv_timer_handler - wywoływać z pętli głównej

// Jasność 0..100. Gdy steady = true, podświetlenie świeci bez PWM (cisza elektryczna w trybie FM).
void setBacklight(uint8_t percent, bool steady);
void off();
bool isOn();

// Poprawki dla konkretnej sztuki ekranu, zapamiętane w pamięci urządzenia (konsola: lcd)
struct Tweaks {
  bool invert = false;  // negatyw -> normalne kolory
  bool flip = false;    // obraz do góry nogami -> obrót o 180 stopni
  bool bgr = false;     // czerwony i niebieski zamienione
};
Tweaks currentTweaks();
void setTweaks(const Tweaks& t);
// Kopia ekranu (RGB565, 320x240, wiersz po wierszu) do podglądu w panelu przeglądarki.
// enableMirror wołać z pętli głównej; mirror() = nullptr, gdy wyłączona albo brak PSRAM.
void enableMirror();
const uint16_t* mirror();
uint32_t mirrorVersion();  // rośnie przy każdej narysowanej zmianie

// Pasy: czerwony, zielony, niebieski, biały, czarny (od lewej). Znikają przy następnym odświeżeniu ekranu.
void testPattern();

}  // namespace display
