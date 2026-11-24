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

}  // namespace display
